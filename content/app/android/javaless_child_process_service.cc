// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/api-level.h>
#include <android/binder_ibinder.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>

#include <charconv>
#include <set>

#include "aidl/org/chromium/base/process_launcher/BnChildProcessService.h"
#include "base/android/android_info.h"
#include "base/android/apk_info.h"
#include "base/android/child_process_service.h"
#include "base/android/command_line_android.h"
#include "base/android/device_info.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/version_info/android/channel_getter.h"
#include "build/build_config.h"
#include "content/app/android/content_main_android.h"
#include "content/common/shared_file_util.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "third_party/android_toolchain/native_service.h"

using aidl::org::chromium::base::library_loader::IRelroLibInfo;
using aidl::org::chromium::base::process_launcher::BnChildProcessService;
using aidl::org::chromium::base::process_launcher::IChildProcessArgs;
using aidl::org::chromium::base::process_launcher::IParentProcess;
using ndk::ScopedAStatus;
using ndk::ScopedFileDescriptor;
using ndk::SpAIBinder;

namespace content {
namespace {

class ChildProcessService : public BnChildProcessService,
                            public base::DelegateSimpleThread::Delegate {
 public:
  ChildProcessService();
  ~ChildProcessService() override;
  ScopedAStatus bindToCaller(const std::string& in_clazz,
                             bool* _aidl_return) override;
  ScopedAStatus setupConnection(
      const IChildProcessArgs& args,
      const std::shared_ptr<IParentProcess>& parentProcess,
      const std::optional<std::vector<SpAIBinder>>& clientInterfaces) override;
  ScopedAStatus forceKill() override;
  ScopedAStatus onMemoryPressure(int32_t pressure) override;
  ScopedAStatus onSelfFreeze() override;
  ScopedAStatus dumpProcessStack() override;
  ScopedAStatus getSourceDir(std::string* _aidl_return) override;
  ScopedAStatus consumeRelroLibInfo(
      const std::optional<IRelroLibInfo>& in_libInfo) override;

  // The function that runs on the renderer main thread.
  void Run() override;
  void SpawnMainThread();

 private:
  std::unique_ptr<base::DelegateSimpleThread> thread_;
  base::Lock bind_to_caller_lock_;
  pid_t bound_calling_pid_ GUARDED_BY(bind_to_caller_lock_) = 0;
  std::string bound_calling_clazz_ GUARDED_BY(bind_to_caller_lock_);

  base::ConditionVariable child_process_args_signal_;
  base::Lock child_process_args_lock_;
  std::unique_ptr<IChildProcessArgs> child_process_args_
      GUARDED_BY(child_process_args_lock_);
  std::shared_ptr<IParentProcess> parent_process_
      GUARDED_BY(child_process_args_lock_);
};

}  // namespace

ChildProcessService::ChildProcessService()
    : child_process_args_signal_(&child_process_args_lock_) {}
ChildProcessService::~ChildProcessService() {}

std::optional<std::map<int, std::string>> GetIdsToKeys() {
  std::string file_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);
  if (!file_switch_value.empty()) {
    return ParseSharedFileSwitchValue(file_switch_value);
  } else {
    return std::nullopt;
  }
}

void RegisterFileDescriptors(const IChildProcessArgs& args) {
  std::vector<int> ids(args.fileDescriptorInfos.size());
  std::vector<int64_t> offsets(args.fileDescriptorInfos.size());
  std::vector<int64_t> sizes(args.fileDescriptorInfos.size());
  std::vector<int> fds(args.fileDescriptorInfos.size());
  std::optional<std::map<int, std::string>> ids_to_keys = GetIdsToKeys();
  std::vector<std::optional<std::string>> keys(ids.size());
  for (size_t i = 0; i < args.fileDescriptorInfos.size(); i++) {
    ids[i] = args.fileDescriptorInfos[i].id;
    offsets[i] = args.fileDescriptorInfos[i].offset;
    sizes[i] = args.fileDescriptorInfos[i].size;
    fds[i] = args.fileDescriptorInfos[i].fd.dup().release();
    if (ids_to_keys) {
      if (auto it = ids_to_keys->find(ids[i]); it != ids_to_keys->end()) {
        keys[i] = it->second;
        continue;
      }
    }
    keys[i] = std::nullopt;
  }
  base::android::RegisterFileDescriptors(keys, ids, fds, offsets, sizes);
}

void SetBuildInfo(const IChildProcessArgs& args) {
  base::android::android_info::Set(args.androidInfo);
  base::android::apk_info::Set(args.apkInfo);
  base::android::device_info::Set(args.deviceInfo);
  version_info::android::SetChannel(
      static_cast<version_info::Channel>(args.channel));
}

// This is intended to be the equivalent to mainThreadMain() in
// ChildProcessService.java.
void ChildProcessService::Run() {
  std::unique_ptr<IChildProcessArgs> args;
  std::shared_ptr<IParentProcess> parent_process;
  {
    base::AutoLock auto_lock(child_process_args_lock_);
    while (!child_process_args_) {
      child_process_args_signal_.Wait();
    }
    args = std::move(child_process_args_);
    parent_process = parent_process_;
  }

  std::vector<std::string> command_line_copy = args->commandLine;
  base::android::CommandLineInit(command_line_copy);

  base::android::LibraryProcessType process_type =
      static_cast<base::android::LibraryProcessType>(args->libraryProcessType);
  if (!NativeInitializationHook(process_type)) {
    LOG(FATAL) << "Failed to initialize native.";
  }
  SetBuildInfo(*args);
  InitChildProcessCommon(args->cpuCount, args->cpuFeatures);

  base::android::LibraryLoaded(process_type);
  base::UmaHistogramBoolean("Android.ChildProcess.JavalessStarted", true);

  RegisterFileDescriptors(*args);
  StartContentMain(false);
  // Content main has finished, the process is exiting.
  parent_process->reportCleanExit();
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

void ChildProcessService::SpawnMainThread() {
  // LINT.IfChange
#if defined(ARCH_CPU_64_BITS)
  size_t stack_size = 8 * 1024 * 1024;
#else
  size_t stack_size = 4 * 1024 * 1024;
#endif
  // LINT.ThenChange(//base/android/java/src/org/chromium/base/process_launcher/ChildProcessService.java)
  // Set up stack size to match Java.
  base::SimpleThread::Options options;
  options.stack_size = stack_size;
  thread_ = std::make_unique<base::DelegateSimpleThread>(this, "CrRendererMain",
                                                         options);
  thread_->StartAsync();
}

ScopedAStatus ChildProcessService::setupConnection(
    const IChildProcessArgs& args,
    const std::shared_ptr<IParentProcess>& parentProcess,
    const std::optional<std::vector<SpAIBinder>>& clientInterfaces) {
  // Entering locked scope for bound_calling_pid.
  {
    base::AutoLock lock(bind_to_caller_lock_);
    if (args.bindToCaller && bound_calling_pid_ == 0) {
      LOG(ERROR) << "Service has not been bound with bindToCaller()";
      parentProcess->finishSetupConnection(-1, 0, 0, std::nullopt);
      return ScopedAStatus::ok();
    }
  }
  {
    base::AutoLock auto_lock(child_process_args_lock_);
    parent_process_ = parentProcess;
    // As IChildProcessArgs is not copy-assignable, due to it embedding
    // ParcelFileDescriptors, we must move it. The input |args| is const, but
    // as this is a one-way binder transaction, we can safely cast away the
    // const and move it.
    child_process_args_ = std::make_unique<IChildProcessArgs>(
        std::move(const_cast<IChildProcessArgs&>(args)));
    child_process_args_signal_.Broadcast();
  }

  parentProcess->finishSetupConnection(base::GetCurrentProcId(), 0, -1,
                                       std::nullopt);
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::bindToCaller(const std::string& in_clazz,
                                                bool* _aidl_return) {
  pid_t calling_pid = AIBinder_getCallingPid();
  base::AutoLock lock(bind_to_caller_lock_);
  *_aidl_return = true;
  if (bound_calling_pid_ == 0 && bound_calling_clazz_.empty()) {
    bound_calling_pid_ = calling_pid;
    bound_calling_clazz_ = in_clazz;
  } else if (bound_calling_pid_ != calling_pid) {
    LOG(ERROR) << "Service is already bound by pid " << bound_calling_pid_
               << ", cannot bind for pid " << calling_pid;
    *_aidl_return = false;
  } else if (bound_calling_clazz_ != in_clazz) {
    LOG(WARNING) << "Service is already bound by " << bound_calling_clazz_
                 << ", cannot bind for " << in_clazz;
    *_aidl_return = false;
  }
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::forceKill() {
  // This matches what we do in Java (Process.killProcess).
  kill(getpid(), SIGKILL);
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::onMemoryPressure(int32_t pressure) {
  // Make sure the renderer main thread has been initialized. If it hasn't, it's
  // probably too early during startup, and notifying memory pressure likely
  // won't work either.
  if (base::SingleThreadTaskRunner::HasMainThreadDefault()) {
    // This logic doesn't match the Java equivalent. In the Java implementation,
    // we assume that the ChildProcessService is getting memory pressure signals
    // from the browser process (this function), and ComponentCallbacks2. We
    // only have signals from the browser process available to a javaless
    // renderer, so we trust what it sends entirely.
    base::MemoryPressureListenerRegistry::NotifyMemoryPressureFromAnyThread(
        static_cast<base::MemoryPressureLevel>(pressure));
  }
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::onSelfFreeze() {
  base::android::OnSelfFreeze();
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::dumpProcessStack() {
  base::android::DumpProcessStack();
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::getSourceDir(std::string* _aidl_return) {
  Dl_info dl_info;
  // Nothing special about SetBuildInfo, just a function already available in
  // this file that is is not a member function (which would make dladdr a bit
  // more annoying to use).
  if (dladdr(reinterpret_cast<const void*>(&SetBuildInfo), &dl_info) == 0 ||
      !dl_info.dli_fname) {
    LOG(ERROR) << "Failed to obtain sourceDir via dladdr";
    return ScopedAStatus::ok();
  }

  std::string_view fname(dl_info.dli_fname);
  // Android's dynamic linker may return paths like
  // "/data/app/.../base.apk!/lib/arm64-v8a/libchrome.so" or
  // "/data/app/.../base.apk". Truncate after ".apk" to match Java's
  // ApplicationInfo.sourceDir.
  size_t ext_pos = fname.find(".apk");
  if (ext_pos != std::string_view::npos) {
    *_aidl_return = std::string(fname.substr(0, ext_pos + 4));
  } else {
    *_aidl_return = std::string(fname);
  }
  return ScopedAStatus::ok();
}

ScopedAStatus ChildProcessService::consumeRelroLibInfo(
    const std::optional<IRelroLibInfo>& in_libInfo) {
  // Not implemented yet. Relro sharing is something we'd like to have, but is
  // significantly more complicated than the prototype we are starting with. See
  // crbug.com/408023044 for tracking.
  return ScopedAStatus::ok();
}

namespace {
std::shared_ptr<content::ChildProcessService>& getChildProcessService() {
  static base::NoDestructor<std::shared_ptr<content::ChildProcessService>>
      service_ptr;
  return *service_ptr.get();
}

std::set<uint64_t>& getBindTokens() {
  static base::NoDestructor<std::set<uint64_t>> tokens;
  return *tokens.get();
}

void onDestroy(ANativeService* service) {}

AIBinder* onBind(ANativeService* service,
                 uint64_t bindToken,
                 char const* action,
                 char const* data) {
  auto& child_process_service = getChildProcessService();
  if (!child_process_service) {
    child_process_service =
        ndk::SharedRefBase::make<content::ChildProcessService>();
    child_process_service->SpawnMainThread();
  }
  getBindTokens().insert(bindToken);
  ::ndk::SpAIBinder spBinder = child_process_service->asBinder();
  AIBinder* result = spBinder.get();
  // Required to do this by the NDK API and is not balanced anywhere.
  AIBinder_incStrong(result);
  return result;
}

void onRebind(ANativeService* service, uint64_t bindToken) {}

bool onUnbind(ANativeService* service, uint64_t bindToken) {
  auto& tokens = getBindTokens();
  tokens.erase(bindToken);
  if (tokens.empty()) {
    getChildProcessService().reset();
  }
  // We return false to ask the OS not to call onRebind on us.
  return false;
}

}  // namespace
}  // namespace content

#define EXPORT_TO_ANDROID extern "C" __attribute__((visibility("default")))

EXPORT_TO_ANDROID void NativeChildProcessService_onCreate(
    ANativeService* service) {
  ANativeService_setOnBindCallback(service, &content::onBind);
  ANativeService_setOnUnbindCallback(service, &content::onUnbind);
  ANativeService_setOnRebindCallback(service, &content::onRebind);
  ANativeService_setOnDestroyCallback(service, &content::onDestroy);
}

// This is a hook for libraries to use who might want something happening very
// early on process start. Note that JNI_OnLoad does not work with javaless
// renderers, so often things you might put there should go into a override of
// this instead.
__attribute__((weak)) bool NativeInitializationHook(
    base::android::LibraryProcessType library_process_type) {
  return false;
}
