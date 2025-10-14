// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/api-level.h>
#include <android/binder_ibinder.h>
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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/simple_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/version_info/android/channel_getter.h"
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
  ScopedAStatus getAppInfoStrings(
      std::vector<std::string>* _aidl_return) override;
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

  base::android::LibraryProcessType process_type =
      static_cast<base::android::LibraryProcessType>(args->libraryProcessType);
  if (!NativeInitializationHook(process_type)) {
    LOG(FATAL) << "Failed to initialize native.";
  }
  SetBuildInfo(*args);
  InitChildProcessCommon(args->cpuCount, args->cpuFeatures);

  std::vector<std::string> command_line_copy = args->commandLine;
  base::android::CommandLineInit(command_line_copy);
  base::android::LibraryLoaded(process_type);

  RegisterFileDescriptors(*args);
  StartContentMain(false);
  // Content main has finished, the process is exiting.
  parent_process->reportCleanExit();
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

void ChildProcessService::SpawnMainThread() {
  thread_ =
      std::make_unique<base::DelegateSimpleThread>(this, "CrRendererMain");
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
  // Need to make sure the threadpool exists. If it doesn't exist, that means we
  // are probably before starting the renderer main thread, and notifying memory
  // pressure likely won't work either.
  if (base::ThreadPoolInstance::Get()) {
    // This logic doesn't match the Java equivalent. In the Java implementation,
    // we assume that the ChildProcessService is getting memory pressure signals
    // from the browser process (this function), and ComponentCallbacks2. We
    // only have signals from the browser process available to a javaless
    // renderer, so we trust what it sends entirely.
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&base::MemoryPressureListener::NotifyMemoryPressure,
                       static_cast<base::MemoryPressureLevel>(pressure)));
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

ScopedAStatus ChildProcessService::getAppInfoStrings(
    std::vector<std::string>* _aidl_return) {
  // Not implemented yet - unsure if this check can work with Javaless
  // renderers, as getting the sourceDir or sharedLibraryFiles are things that
  // aren't exposed to the NDK.
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

std::set<int32_t>& getIntentTokens() {
  static base::NoDestructor<std::set<int32_t>> tokens;
  return *tokens.get();
}

void onDestroy(ANativeService* service) {}

AIBinder* onBind(ANativeService* service,
                 int32_t intentToken,
                 char const* action,
                 char const* data) {
  auto& child_process_service = getChildProcessService();
  if (!child_process_service) {
    child_process_service =
        ndk::SharedRefBase::make<content::ChildProcessService>();
    child_process_service->SpawnMainThread();
  }
  getIntentTokens().insert(intentToken);
  ::ndk::SpAIBinder spBinder = child_process_service->asBinder();
  AIBinder* result = spBinder.get();
  // Required to do this by the NDK API and is not balanced anywhere.
  AIBinder_incStrong(result);
  return result;
}

void onRebind(ANativeService* service, int32_t intentToken) {}

bool onUnbind(ANativeService* service, int32_t intentToken) {
  auto& tokens = getIntentTokens();
  tokens.erase(intentToken);
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
