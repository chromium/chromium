// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <dawn/dawn_proc.h>
#include <dawn/webgpu_cpp.h>
#include <memory>
#include <vector>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "command_buffer/common/constants.h"
#include "command_buffer/common/sync_token.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/cmd_buf_lpm_fuzz.h"
#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/cmd_buf_lpm_fuzz.pb.h"
#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/webgpu_support.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace gpu::cmdbuf::fuzzing {

/* One-time fuzzer initialization. Called only once during fuzzer startup. */
CmdBufFuzz::CmdBufFuzz() : base::TestSuite(0, (char**)nullptr) {
  CommandLineInit();
  GfxInit();
}

/* Enable WebGPU features and disable GPU hardware acceleration. */
void CmdBufFuzz::CommandLineInit() {
  [[maybe_unused]] auto* command_line = base::CommandLine::ForCurrentProcess();
  //--disable-gpu to force software rendering
  command_line->AppendSwitchASCII(switches::kDisableGpu, "1");
  // --use-webgpu-adapter=swiftshader
  command_line->AppendSwitchASCII(
      switches::kUseWebGPUAdapter,
      switches::kVulkanImplementationNameSwiftshader);
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && BUILDFLAG(USE_DAWN)
  //--enable-features=Vulkan
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  gl::kANGLEImplementationVulkanName);
  //--use-vulkan=swiftshader
  command_line->AppendSwitchASCII(
      switches::kUseVulkan, switches::kVulkanImplementationNameSwiftshader);
#endif
}

/* Initialize the graphics stack in single-process mode with WebGPU. */
void CmdBufFuzz::GfxInit() {
  VLOG(3) << "GfxInit() started";

  VLOG(3) << "Detecting platform specific features and setting preferences "
             "accordingly";
  gpu::GpuPreferences preferences;
  preferences.enable_webgpu = true;
  preferences.disable_software_rasterizer = false;
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && BUILDFLAG(USE_DAWN)
  preferences.gr_context_type = gpu::GrContextType::kVulkan;
  // Use SwiftShader so fuzzing can work without a physical GPU.
  preferences.use_vulkan = gpu::VulkanImplementationName::kSwiftshader;
#endif
  preferences.use_webgpu_adapter = WebGPUAdapterName::kSwiftShader;
  preferences.enable_unsafe_webgpu = true;
  preferences.enable_gpu_service_logging_gpu = true;

  // Initializing some portion of Chromium's windowing feature seems to be
  // required to use gpu::CreateBufferUsageAndFormatExceptionList().

  // TODO(bookholt): It's not obvious whether having legit values from
  // gpu::CreateBufferUsageAndFormatExceptionList() is really desired for
  // fuzzing, but it's a starting point.

  // TODO(bookholt): OS specific windowing init.
  VLOG(3) << "Aura + Ozone init";
  gl_display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithStubBindings();
  CHECK(gl_display_);
  VLOG(3) << "TestGpuServiceHolder: starting GPU threads";
  gpu_service_holder_ =
      std::make_unique<viz::TestGpuServiceHolder>(preferences);

  VLOG(3) << "Init WebGPU context";
  // TODO(bookholt): We might want to fuzz or tune some Context attributes, but
  // for now we set them to enable initialization of a valid WebGPU context.
  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;
  shared_memory_limits_ = std::make_unique<SharedMemoryLimits>(
      SharedMemoryLimits::ForWebGPUContext());
  /*
     WebGPUInProcessContext does a lot of handy setup and uses threads to
     simulate cross-process communication in a single process binary, but it's
     not a 100% match to a real browser instance. E.g. it uses a char* as
     backing memory for the CommandBuffer's RingBuffer instead of a
     platform-specific shared memory implementation, and it bypasses Mojo
     entirely, but it's compatible with our single-process fuzze_test build
     target, so it's a decent starting point.

     The thinking is to let WebGPUInProcessContext create a working WebGPU
     Context with associated data structures and then we'll tune or replace
     anything we want to fuzz. However, starting from data structures with
     benign properties may turn out to unnecessarily limit mischief, so in the
     future it may be necessary to do more of our own Context init.

     Data structures of particular interest to fuzzing include:
     - CommandBuffer and associated TransferBuffers
     - WebGPUDecoder / DecoderContext
  */
  webgpu_context_ = std::make_unique<WebGPUInProcessContext>();
  ContextResult webgpu_context_result = webgpu_context_->Initialize(
      gpu_service_holder_->task_executor(), attributes, *shared_memory_limits_);
  CHECK_EQ(webgpu_context_result, ContextResult::kSuccess);

  VLOG(3) << "Wire protocol setup";
  wire_channel_ = webgpu()->GetAPIChannel();
  dawnProcSetProcs(&dawn::wire::client::GetProcs());
  dawn_wire_services_ =
      static_cast<webgpu::DawnWireServices*>(wire_channel_.get());
  dawn_wire_serializer_ = dawn_wire_services_->serializer();
  wire_descriptor_ = std::make_unique<dawn::wire::WireServerDescriptor>();
  wire_descriptor_->procs = &dawn::wire::client::GetProcs();
  wire_descriptor_->serializer = dawn_wire_serializer_.get();
  wire_server_ = std::make_unique<dawn::wire::WireServer>(*wire_descriptor_);
  dawn_instance_ = std::make_unique<dawn::native::Instance>();
  wire_server_->InjectInstance(dawn_instance_->Get(), {1, 0});

  VLOG(3) << "Populate data structure grab bag";
  command_buffer_ = webgpu_context_->GetCommandBufferForTest();
  command_buffer_service_ =
      std::make_unique<CommandBufferService>(command_buffer_, nullptr);
  cmd_helper_ = webgpu_context_->GetCommandBufferHelperForTest();
  webgpu_cmd_helper_ =
      std::make_unique<webgpu::WebGPUCmdHelper>(command_buffer_.get());
  task_executor_ = command_buffer_->service_for_testing();
  // task_executor_->GetSharedContextState()->surface();
  surface_ = gl::init::CreateOffscreenGLSurface(gl_display_, gfx::Size());
  CHECK(surface_.get());
  decoder_ = command_buffer_->GetWebGPUDecoderForTest();
  webgpu_instance_ = wgpu::Instance(wire_channel_->GetWGPUInstance());
  buffer_ = cmd_helper_->get_ring_buffer();
  CHECK(buffer_);
  command_buffer_id_ = cmd_helper_->get_ring_buffer_id();
  // command_buffer_->SetGetBuffer(command_buffer_id_);
  webgpu_impl_ = webgpu_context_->GetImplementation();

  VLOG(3) << "GfxInit complete";
}

CmdBufFuzz::~CmdBufFuzz() = default;

/* Per-test case setup. */
void CmdBufFuzz::RuntimeInit() {
  // Verify command buffer is ready for the new test case.
  if (!cmd_helper_->HaveRingBuffer()) {
    VLOG(3) << "CommandBuffer (re-)init";
    cmd_helper_->Initialize(shared_memory_limits_->command_buffer_size);

    buffer_ = cmd_helper_->get_ring_buffer();
    CHECK(buffer_);
    command_buffer_id_ = cmd_helper_->get_ring_buffer_id();
    command_buffer_->SetGetBuffer(command_buffer_id_);
  }
}

/* Deserialize a proto message from a test case into a gpu::SyncToken. */
gpu::SyncToken CmdBufFuzz::SyncTokenFromProto(fuzzing::SyncToken token_proto) {
  // TODO(bookholt): Pick buffer_id from a narrower range of sensible values.
  CommandBufferId buffer_id =
      CommandBufferId::FromUnsafeValue(token_proto.command_buffer_id());
  uint64_t release_count = token_proto.release_count();

  // Limit the range of CommandBufferNamespaceId values because this fuzzer
  // bypasses Mojo trait validation, but a real renderer cannot.
  CommandBufferNamespace ns = static_cast<CommandBufferNamespace>(
      token_proto.namespace_id() %
      gpu::cmdbuf::fuzzing::CommandBufferNamespaceIds::MAX_VALID);

  gpu::SyncToken sync_token(ns, buffer_id, release_count);
  return sync_token;
}

/* Dummy SignalSyncToken callback function. */
void CmdBufFuzz::SignalSyncTokenCallback() {}

/* Fuzzing happens here. */
void CmdBufFuzz::RunCommandBuffer(fuzzing::CmdBufSession session) {
  if (!session.actions_size()) {
    VLOG(3) << "Empty test case :(";
    return;
  }

  RuntimeInit();

  while (action_index_ < session.actions_size()) {
    const auto& action = session.actions(action_index_);
    switch (action.action_case()) {
      case fuzzing::Action::kCmdBufOp: {
        auto op = action.cmdbufop();
        switch (op.cmd_buf_ops_case()) {
          case fuzzing::InProcessCommandBufferOp::kGetLastState: {
            VLOG(3) << "kGetLastState";
            command_buffer_->GetLastState();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kFlush: {
            VLOG(3) << "kFlush";
            command_buffer_->Flush(op.flush().put_offset());
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOrderingBarrier: {
            VLOG(3) << "kOrderingBarrier";
            command_buffer_->OrderingBarrier(op.orderingbarrier().put_offset());
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kWaitForTokenInRange: {
            VLOG(3) << "kWaitForTokenInRange";
            // It'd be nice to fuzz this, but it's currently too slow.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kWaitForGetOffsetInRange: {
            VLOG(3) << "kWaitForGetOffsetInRange";
            // It'd be nice to fuzz this, but it's currently too slow.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kSetGetBuffer: {
            VLOG(3) << "kSetGetBuffer";
            // TODO(bookholt): Random shm_id is probably unwise.
            command_buffer_->SetGetBuffer(op.setgetbuffer().shm_id());
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kCreateTransferBuffer: {
            VLOG(3) << "kCreateTransferBuffer";
            // TODO(bookholt): Add to vector for use in kDestroyTransferBuffer.
            int id = -1;
            command_buffer_->CreateTransferBuffer(
                op.createtransferbuffer().size(), &id);
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kDestroyTransferBuffer: {
            VLOG(3) << "kDestroyTransferBuffer";
            // TODO(bookholt): Make a way to identify all TransferBuffers
            // created during GfxInit(), then add those to the TB vector.
            //
            // TODO(bookholt): Pull TB IDs from the vector rather than using
            // totally random IDs that are unlikely to be valid.
            command_buffer_->DestroyTransferBuffer(
                op.destroytransferbuffer().id());
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kForceLostContet: {
            VLOG(3) << "kForceLostContet";
            auto reason = static_cast<error::ContextLostReason>(
                op.forcelostcontet().reason() %
                    error::ContextLostReason::kContextLostReasonLast +
                1);
            command_buffer_->ForceLostContext(reason);
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kSetGpuControlClient: {
            // TODO(bookholt): find a sensible approach
            VLOG(3) << "kSetGpuControlClient: unsupported op";
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetCapabilities: {
            VLOG(3) << "kGetCapabilities: NOOP";
            // Kind of a NOOP for fuzzing. :-/
            command_buffer_->GetCapabilities();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kSignalQuery: {
            VLOG(3) << "kSignalQuery: unsupported op";
            // Signal Queries not supported by WebGPUDecoderImpl
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kCreateGpuFence: {
            VLOG(3) << "kCreateGpuFence: unsupported op";
            // GPU Fence not supported by WebGPUDecoderImpl
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetGpuFence: {
            VLOG(3) << "kGetGpuFence: unsupported op";
            // GPU Fence not supported by WebGPUDecoderImpl
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kSetLock: {
            VLOG(3) << "kSetLock: unsupported op";
            // Interesting feature to fuzz, but unsupported by
            // InProcessCommandBuffer :( TODO(bookholt): support for SetLock
            // would require either
            // - adding libfuzzer support to BrowserTests or
            // - snapshot fuzzing support
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kEnsureWorkVisible: {
            VLOG(3) << "kEnsureWorkVisible: unsupported op";
            // Unsupported by InProcessCommandBuffer :(
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetNamespaceID: {
            VLOG(3) << "kGetNamespaceID: unsupported op";
            // Valid values in gpu/command_buffer/common/constants.h Calling
            // this function is a NOOP from a fuzzer perspective, but other
            // CommandBufferOps will want to implement it.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetCommandBufferID: {
            VLOG(3) << "kGetCommandBufferID: unsupported op";
            // Calling this function is a NOOP from a fuzzer perspective, but
            // other CommandBufferOps will want to implement it.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kFlushPendingWork: {
            VLOG(3) << "kFlushPendingWork: unsupported op";
            // Unsupported by InProcessCommandBuffer :(
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGenerateFenceSyncRelease: {
            VLOG(3) << "kGenerateFenceSyncRelease";
            command_buffer_->GenerateFenceSyncRelease();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kIsFenceSyncReleased: {
            VLOG(3) << "kIsFenceSyncReleased: unsupported op";
            // Calling this function directly is effectively a NOOP, but other
            // CommandBufferOps may want to implement it.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kSignalSyncToken: {
            VLOG(3) << "kSignalSyncToken";
            gpu::SyncToken sync_token =
                SyncTokenFromProto(op.signalsynctoken().sync_token());

            auto callback = base::BindOnce(&CmdBufFuzz::SignalSyncTokenCallback,
                                           base::Unretained(this));
            command_buffer_->SignalSyncToken(std::move(sync_token),
                                             std::move(callback));
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kWaitSyncToken: {
            VLOG(3) << "kWaitSyncToken";
            gpu::SyncToken sync_token =
                SyncTokenFromProto(op.waitsynctoken().sync_token());

            command_buffer_->WaitSyncToken(std::move(sync_token));
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kCanWaitUnverifiedSyncToken: {
            VLOG(3) << "kCanWaitUnverifiedSyncToken: unsupported op";
            // NOOP
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnCommandBatchProcessed: {
            VLOG(3) << "kOnCommandBatchProcessed: unsupported op";
            // ~NOOP
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnParseError: {
            VLOG(3) << "kOnParseError: unsupported op";
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnConsoleMessage: {
            // Not supported by InProcessCommandBuffer.
            VLOG(3) << "kOnConsoleMessage: unsupported op";
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kCacheBlob: {
            // Not supported by InProcessCommandBuffer.
            VLOG(3) << "kCacheBlob: unsupported op";
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnFenceSyncRelease: {
            VLOG(3) << "kOnFenceSyncRelease";
            // GPU Fence not supported by WebGPUDecoderImpl
            command_buffer_->OnFenceSyncRelease(
                op.onfencesyncrelease().release());
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnDescheduleUntilFinished: {
            VLOG(3) << "kOnDescheduleUntilFinished: unsupported op";
            // Not supported by InProcessCommandBuffer.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnRescheduleAfterFinished: {
            VLOG(3) << "kOnRescheduleAfterFinished: unsupported op";
            // Not supported by InProcessCommandBuffer.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kOnSwapBuffers: {
            VLOG(3) << "kOnSwapBuffers: unsupported op";
            // Not supported by InProcessCommandBuffer.
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kScheduleGrContextCleanup: {
            VLOG(3) << "kScheduleGrContextCleanup";
            command_buffer_->ScheduleGrContextCleanup();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kHandleReturnData: {
            VLOG(3) << "kHandleReturnData: "
                    << op.handlereturndata().data().size();

            std::vector<uint8_t> vec;
            for (unsigned int entry : op.handlereturndata().data()) {
              vec.push_back(entry);
            }
            base::span<uint8_t> data_span(vec);
            // Passing totally unstructured data leads to hitting validation
            // errors in webgpu_decoder_impl.cc.

            // We don't fuzz HandleReturnData because, as of right now, that
            // command is exclusively used by the client in the renderer.
            // {GPU Process}->{Renderer Process} attacks are not in our threat
            // model.
            // command_buffer_->HandleReturnData(data_span);
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetFeatureInfo: {
            VLOG(3) << "kGetFeatureInfo";
            command_buffer_->GetFeatureInfo();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetGpuFeatureInfo: {
            VLOG(3) << "kGetGpuFeatureInfo";
            command_buffer_->GetGpuFeatureInfo();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetTransferCacheForTest: {
            VLOG(3) << "kGetTransferCacheForTest";
            // TODO(bookholt): Worth a think about refactoring to make use of
            // the TransferCache.
            command_buffer_->GetTransferCacheForTest();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetRasterDecoderIdForTest: {
            VLOG(3) << "kGetRasterDecoderIdForTest";
            command_buffer_->GetRasterDecoderIdForTest();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetWebGPUDecoderForTest: {
            VLOG(3) << "kGetWebGPUDecoderForTest";
            command_buffer_->GetWebGPUDecoderForTest();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kServiceForTesting: {
            VLOG(3) << "kServiceForTesting";
            command_buffer_->service_for_testing();
            break;
          }

          case fuzzing::InProcessCommandBufferOp::kGetSharedImageInterface: {
            VLOG(3) << "kGetSharedImageInterface: unsupported op";
            // TODO(bookholt): Worth a think about refactoring to make use of
            // the gpu::SharedImageInterface.
            command_buffer_->GetSharedImageInterface();
            break;
          }

          // TODO(bookholt): The fuzzer really shouldn't generate this value,
          // but it does. Investigate how/why LPM generates this value and
          // consider tuning to avoid it because it seems like a waste of
          // performance.
          case fuzzing::InProcessCommandBufferOp::CMD_BUF_OPS_NOT_SET: {
            VLOG(3) << "Invalid InProcessCommandBufferOp";
            break;
          }
        }
        break;
      }
      // TODO(bookholt): The fuzzer really shouldn't generate this value, but it
      // does. Investigate how/why LPM generates this value and consider tuning
      // to avoid it because it seems like a waste of performance.
      case fuzzing::Action::ACTION_NOT_SET: {
        DLOG(WARNING) << "No Action set";
      } break;
    }
    action_index_++;
  }

  //  Prepare for next testcase.
  if (!Reset()) {
    // See crbug.com/1424591
    VLOG(1) << "Test case reset failed. Re-initializing graphics.";
    // Re-initialize the graphics stack to (hopefully!) avoid sync problems.
    GfxInit();
    return;
  }
}

/* Make CommandBuffer ready for the next test case. */
bool CmdBufFuzz::CmdBufReset() {
  // Wait for handling of commands from the previous test case to complete.
  return cmd_helper_->Finish();
}

/* Clean up from the last test case and get ready for the next one. */
bool CmdBufFuzz::Reset() {
  action_index_ = 0;
  return CmdBufReset();
}

// Keeper of global fuzzing state
CmdBufFuzz* wgpuf_setup;

}  // namespace gpu::cmdbuf::fuzzing

/* One-time early initialization at process startup. */
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  logging::SetMinLogLevel(logging::LOGGING_VERBOSE);  // Enable logging.
  CHECK(base::i18n::InitializeICU());  // Unicode support for cmd line parsing.
  base::CommandLine::Init(*argc, *argv);  // Parse cmd line args.
  mojo::core::Init();  // Initialize Mojo; probably unnecessary.

  gpu::cmdbuf::fuzzing::wgpuf_setup = new gpu::cmdbuf::fuzzing::CmdBufFuzz();
  return 0;
}

/* Fuzzer entry point. Called on every iteration with a fresh test case. */
DEFINE_PROTO_FUZZER(gpu::cmdbuf::fuzzing::CmdBufSession& session) {
  gpu::cmdbuf::fuzzing::wgpuf_setup->RunCommandBuffer(session);
  return;
}
