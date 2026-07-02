// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MojoLPM fuzzer for gpu::SharedImageStub::ExecuteDeferredRequest.
//
// SharedImageStub is the GPU-process endpoint that services the batched
// DeferredSharedImageRequest union sent by every renderer/compositor over the
// GpuChannel (FlushDeferredRequests -> GpuChannel::ExecuteDeferredRequest ->
// SharedImageStub::ExecuteDeferredRequest). It owns the per-channel
// SharedImageFactory and therefore the lifetime of every cross-process
// SharedImage backing (IOSurface on macOS, NativePixmap on Ozone, DXGI on
// Windows, etc.). There is no existing fuzzer over this surface.
//
// This harness stands up an in-process GPU service using the same scaffolding
// as the GPU channel unit tests (GpuChannelTestCommon, which performs the
// in-process GpuInit / GL bring-up with null-ANGLE or stub bindings), obtains
// the real SharedImageStub that GpuChannel creates during channel
// establishment, and calls ExecuteDeferredRequest() directly with mojolpm-
// deserialized requests, bypassing the Mojo pipe like the layer_context_impl
// reference fuzzer, so malformed fuzz data exercises the handler instead of
// disconnecting the endpoint.
//
// Bias: the seed corpus and the mutation surface favour create / destroy /
// reuse-mailbox-id sequences, and mailboxes are clamped into a tiny id space so
// the fuzzer naturally produces "destroy a backing then keep referencing the
// same mailbox" interleavings, the IOSurface-backing UAF family.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom-mojolpm.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/test_support/validation_errors_test_util.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "gpu/ipc/service/shared_image_stub_mojolpm_fuzzer.pb.h"

namespace {

// Keep the mailbox id space tiny so create / destroy / re-reference requests
// collide on the same backing, which is what surfaces use-after-free /
// double-free bugs in the IOSurface-backed SharedImage lifetime.
constexpr int8_t kMaxMailboxes = 16;

// Size of the read-only upload buffer pre-registered for CreateSharedImageWith
// Data requests. Clamped to 4MB to bound per-iteration cost.
constexpr size_t kUploadBufferSize = 4 * 1024 * 1024;

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

// Exposes GpuChannelTestCommon's protected channel-creation helpers, mirroring
// the LayerContextImplTestForFuzzing pattern in the reference fuzzer. We never
// call TestBody(); we just reuse the in-process GPU bring-up.
//
// IMPORTANT: GpuChannelTestCommon constructs the process-wide TaskEnvironment
// AND performs the one-off GL bring-up (InitializeOneOff*) plus GpuChannel
// Manager creation in its constructor. Both are process-global and must NOT be
// repeated per fuzzer iteration, so a single instance is owned by the static
// FuzzerEnvironment below and reused. Each iteration only establishes and tears
// down a fresh GpuChannel (with a fresh client id) and its SharedImageStub.
class SharedImageStubTestForFuzzing : public gpu::GpuChannelTestCommon {
 public:
  SharedImageStubTestForFuzzing()
      : gpu::GpuChannelTestCommon(/*use_stub_bindings=*/true) {}

  void TestBody() override {}

  // The channel creates its SharedImageStub during EstablishChannel, so the
  // stub is live immediately after CreateChannel(). Returns nullptr on failure.
  gpu::SharedImageStub* CreateStub(int32_t client_id) {
    last_client_id_ = client_id;
    gpu::GpuChannel* channel = CreateChannel(client_id, /*is_gpu_host=*/false);
    if (!channel) {
      last_client_id_ = -1;
      return nullptr;
    }
    return channel->shared_image_stub();
  }

  // Re-fetch the live stub by client id. Returns nullptr if the channel was
  // torn down mid-testcase (e.g. SharedImageStub::OnError -> GpuChannel::Stop),
  // which faithfully models the real flow: GpuChannel schedules each deferred
  // request as a separate WeakPtr(channel)-bound Scheduler task, so once
  // OnError destroys the channel its remaining tasks are silently dropped and
  // no further request reaches the freed stub. Calling a cached raw stub
  // pointer across actions instead would manufacture a harness-only
  // use-after-free that no renderer could reach.
  gpu::SharedImageStub* GetLiveStub() {
    if (last_client_id_ < 0) {
      return nullptr;
    }
    gpu::GpuChannel* channel =
        channel_manager()->LookupChannel(last_client_id_);
    return channel ? channel->shared_image_stub() : nullptr;
  }

  void DestroyStub() {
    if (last_client_id_ >= 0) {
      channel_manager()->RemoveChannel(last_client_id_);
      last_client_id_ = -1;
      // Let channel/stub destruction (which can post GL work) settle.
      task_environment().RunUntilIdle();
    }
  }

 private:
  int32_t last_client_id_ = -1;
};

// Process-global, established exactly once. Owns the persistent GPU test
// fixture (and thus the single TaskEnvironment + GL bring-up).
struct FuzzerEnvironment {
  FuzzerEnvironment() {
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    mojo::core::Init();
    fixture = std::make_unique<SharedImageStubTestForFuzzing>();
  }
  // GL bring-up (AddGLNativeLibrary) and other base/ services register at-exit
  // callbacks, which CHECK for a live AtExitManager. Declared before `fixture`
  // so it is constructed first (before the GL init in the body) and destroyed
  // last (outliving channel/GL teardown).
  base::AtExitManager at_exit_manager;
  // Make mojo outgoing-message serialization failures non-fatal. mojolpm can
  // build a malformed message (e.g. a Mailbox whose fixed array<int8,16> name
  // is not 16 long, frequently produced via mojolpm's runtime instance system
  // rather than an inline proto field we could pre-clamp). Without this observer
  // alive, mojo CHECK-aborts during the harness's own proto-to-mojo conversion,
  // which is a tooling artifact rather than a SharedImageStub bug. With it
  // alive, the warning is recorded and conversion fails gracefully so malformed
  // inputs still reach the handler. ASAN catches real memory bugs independently.
  // TODO(crbug.com/507724733): Remove this once mojolpm handles fixed-size
  // arrays without an observer.
  mojo::internal::SerializationWarningObserverForTesting
      serialization_warning_observer;
  std::unique_ptr<SharedImageStubTestForFuzzing> fixture;
};

FuzzerEnvironment* g_env = nullptr;

// Restricts a native Mailbox to the small id space defined by kMaxMailboxes so
// distinct requests alias the same backing.
void ClampMailbox(gpu::Mailbox* mailbox) {
  int8_t id = mailbox->name[0];
  id %= kMaxMailboxes;
  if (id < 0) {
    id += kMaxMailboxes;
  }
  mailbox->SetZero();
  mailbox->name[0] = id;
}

// Clamps the just-deserialized native request in place: bounds mailbox ids and
// caps the pixel-data window so CreateSharedImageWithData stays within the
// pre-registered 4MB upload buffer. Operating on the native object (rather than
// guessing mojolpm proto accessor names) keeps this stable across grammar
// regeneration, the same approach as ClampLayerTreeUpdate in the reference.
void ClampRequest(gpu::mojom::DeferredSharedImageRequest* request) {
  using Tag = gpu::mojom::DeferredSharedImageRequest::Tag;
  switch (request->which()) {
    case Tag::kCreateSharedImage:
      ClampMailbox(&request->get_create_shared_image()->mailbox);
      break;
    case Tag::kCreateSharedImageWithData: {
      auto& p = *request->get_create_shared_image_with_data();
      ClampMailbox(&p.mailbox);
      if (p.pixel_data_offset > kUploadBufferSize) {
        p.pixel_data_offset = 0;
      }
      if (p.pixel_data_size > kUploadBufferSize - p.pixel_data_offset) {
        p.pixel_data_size = kUploadBufferSize - p.pixel_data_offset;
      }
      break;
    }
    case Tag::kCreateSharedImageWithBuffer:
      ClampMailbox(&request->get_create_shared_image_with_buffer()->mailbox);
      break;
    case Tag::kUpdateSharedImage:
      ClampMailbox(&request->get_update_shared_image()->mailbox);
      break;
    case Tag::kAddReferenceToSharedImage:
      ClampMailbox(&request->get_add_reference_to_shared_image()->mailbox);
      break;
    case Tag::kCopyToGpuMemoryBuffer:
      ClampMailbox(&request->get_copy_to_gpu_memory_buffer()->mailbox);
      break;
    case Tag::kDestroySharedImage: {
      gpu::Mailbox m = request->get_destroy_shared_image();
      ClampMailbox(&m);
      request->set_destroy_shared_image(m);
      break;
    }
    // register_upload_buffer is intentionally never forwarded from the fuzzer:
    // the harness owns the upload buffer (see below) so a fuzzer-supplied
    // region cannot replace it. nop / pool create+destroy carry no mailbox to
    // clamp.
    default:
      break;
  }
}

// gpu::Mailbox's mojom field is a fixed `array<int8, 16> name`, but mojolpm
// generates a variable-length `m_name`. During FromProto the mojo Serializer
// runs GetArrayValidator<16> and aborts ("deadly signal") whenever the length
// is not exactly 16, crashing in the harness's own proto-to-mojo conversion
// before SharedImageStub ever runs (a tooling artifact rather than a real bug).
// Recursively force every Mailbox_ProtoStruct's m_name to exactly 16 entries
// before conversion, preserving the leading bytes so mailbox-id entropy (and
// the ClampMailbox destroy/reuse collision bias) is retained.
void NormalizeMailboxes(google::protobuf::Message* msg) {
  const google::protobuf::Descriptor* desc = msg->GetDescriptor();
  const google::protobuf::Reflection* refl = msg->GetReflection();
  if (desc->name() == "Mailbox_ProtoStruct") {
    // mojolpm encodes the fixed `array<int8,16> name` as a REQUIRED nested
    // `Name_Array` message holding `repeated Name_ArrayEntry values`. The mojo
    // serializer needs exactly 16. Create m_name if unset, then pad/truncate
    // its `values` to 16 (preserving leading entries so mailbox-id entropy
    // survives).
    const google::protobuf::FieldDescriptor* name_f =
        desc->FindFieldByName("m_name");
    if (name_f && name_f->cpp_type() ==
                      google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      google::protobuf::Message* arr = refl->MutableMessage(msg, name_f);
      const google::protobuf::Reflection* arr_refl = arr->GetReflection();
      const google::protobuf::FieldDescriptor* vals_f =
          arr->GetDescriptor()->FindFieldByName("values");
      if (vals_f && vals_f->is_repeated() &&
          vals_f->cpp_type() ==
              google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
        while (arr_refl->FieldSize(*arr, vals_f) > 16) {
          arr_refl->RemoveLast(arr, vals_f);
        }
        while (arr_refl->FieldSize(*arr, vals_f) < 16) {
          google::protobuf::Message* entry = arr_refl->AddMessage(arr, vals_f);
          const google::protobuf::FieldDescriptor* v =
              entry->GetDescriptor()->FindFieldByName("value");
          if (v && v->cpp_type() ==
                       google::protobuf::FieldDescriptor::CPPTYPE_INT32) {
            entry->GetReflection()->SetInt32(entry, v, 0);
          }
        }
      }
    }
  }
  for (int i = 0; i < desc->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* f = desc->field(i);
    if (f->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      continue;
    }
    if (f->is_repeated()) {
      const int n = refl->FieldSize(*msg, f);
      for (int j = 0; j < n; ++j) {
        NormalizeMailboxes(refl->MutableRepeatedMessage(msg, f, j));
      }
    } else if (refl->HasField(*msg, f)) {
      NormalizeMailboxes(refl->MutableMessage(msg, f));
    }
  }
}

class SharedImageStubTestcase
    : public mojolpm::Testcase<gpu::fuzzing::shared_image_stub::proto::Testcase,
                               gpu::fuzzing::shared_image_stub::proto::Action> {
 public:
  using ProtoTestcase = gpu::fuzzing::shared_image_stub::proto::Testcase;
  using ProtoAction = gpu::fuzzing::shared_image_stub::proto::Action;

  explicit SharedImageStubTestcase(const ProtoTestcase& testcase)
      : mojolpm::Testcase<ProtoTestcase, ProtoAction>(testcase) {}

  void SetUp(base::OnceClosure done_closure) override {
    // Establish a fresh channel + stub on the persistent fixture. Use a unique
    // client id per iteration so re-establishment never collides.
    static int32_t s_next_client_id = 1;
    g_env->fixture->CreateStub(s_next_client_id++);

    // Pre-register a zero-filled 4MB read-only upload buffer so
    // CreateSharedImageWithData has valid backing memory to read from. This
    // takes the real OnRegisterSharedImageUploadBuffer path on the live stub.
    if (gpu::SharedImageStub* stub = g_env->fixture->GetLiveStub()) {
      base::MappedReadOnlyRegion shm =
          base::ReadOnlySharedMemoryRegion::Create(kUploadBufferSize);
      if (shm.IsValid()) {
        gpu::mojom::DeferredSharedImageRequestPtr reg =
            gpu::mojom::DeferredSharedImageRequest::NewRegisterUploadBuffer(
                std::move(shm.region));
        stub->ExecuteDeferredRequest(std::move(reg));
      }
    }

    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void TearDown(base::OnceClosure done_closure) override {
    g_env->fixture->DestroyStub();
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void RunAction(const ProtoAction& action,
                 base::OnceClosure run_closure) override {
    switch (action.action_case()) {
      case ProtoAction::kExecuteDeferredRequest: {
        gpu::mojom::DeferredSharedImageRequestPtr request;
        // Re-fetch the stub each action: a prior action's OnError may have torn
        // down the channel (freeing the stub). GetLiveStub() returns nullptr
        // then, matching the real WeakPtr(channel)-gated scheduler flow where
        // post-teardown deferred requests are dropped, so we never call a freed
        // stub (which would be a harness-only use-after-free).
        gpu::SharedImageStub* stub = g_env->fixture->GetLiveStub();
        if (stub &&
            mojolpm::FromProto(action.execute_deferred_request().request(),
                               request) &&
            request &&
            // The harness owns the upload buffer (registered once in SetUp);
            // drop any fuzzer-supplied RegisterUploadBuffer so it cannot
            // replace the valid backing that CreateSharedImageWithData reads
            // from. This is the suppression the ClampRequest comment refers to.
            request->which() != gpu::mojom::DeferredSharedImageRequest::Tag::
                                    kRegisterUploadBuffer) {
          ClampRequest(request.get());
          // Direct call, bypassing the Mojo pipe, like the reference fuzzer.
          stub->ExecuteDeferredRequest(std::move(request));
        }
        break;
      }
      case ProtoAction::ACTION_NOT_SET:
        break;
    }
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
  }
};

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(
    const gpu::fuzzing::shared_image_stub::proto::Testcase& testcase) {
  // mojolpm dispatches an action only when actions, sequences, AND
  // sequence_indexes are all non-empty (see mojolpm.h IsFinished); a testcase
  // missing any of the three runs nothing, so skip it before paying the
  // one-time environment/GL bring-up cost below.
  if (!testcase.actions_size() || !testcase.sequences_size() ||
      !testcase.sequence_indexes_size()) {
    return;
  }

  // Establish the process-global environment exactly once. Constructing it
  // builds the single TaskEnvironment (via the owned GpuChannelTestCommon
  // fixture) and the one-off GL bring-up, so the current-thread task runner
  // used below is valid. Subsequent iterations reuse the same fixture.
  static base::NoDestructor<FuzzerEnvironment> env;
  g_env = env.get();

  // Pin every Mailbox's name to 16 bytes so mojolpm's proto->mojo conversion
  // can't abort on a malformed fixed-array (the dominant tooling
  // false-positive).
  gpu::fuzzing::shared_image_stub::proto::Testcase normalized = testcase;
  NormalizeMailboxes(&normalized);

  SharedImageStubTestcase testcase_runner(normalized);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<SharedImageStubTestcase>,
                     base::Unretained(&testcase_runner), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
