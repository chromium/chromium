// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cocoa/system_hotkey_helper_mac.h"

#include "base/files/file_path_watcher.h"
#include "base/mac/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/cocoa/system_hotkey_map.h"

namespace {

constexpr auto* kSystemHotkeyPlistPath =
    "Preferences/com.apple.symbolichotkeys.plist";

base::FilePath* SymbolicHotkeysPlistFilePath() {
  static base::NoDestructor<base::FilePath> instance;
  return instance.get();
}

class SystemHotkeyMapManager {
 public:
  SystemHotkeyMapManager()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::MayBlock()})),
        file_path_watcher_(std::make_unique<base::FilePathWatcher>()) {
    if (SymbolicHotkeysPlistFilePath()->empty())
      *SymbolicHotkeysPlistFilePath() =
          base::mac::GetUserLibraryPath().Append(kSystemHotkeyPlistPath);
  }

  SystemHotkeyMapManager(SystemHotkeyMapManager&&);

  SystemHotkeyMapManager(const SystemHotkeyMapManager&) = delete;
  SystemHotkeyMapManager& operator=(const SystemHotkeyMapManager&) = delete;

  ~SystemHotkeyMapManager() {
    task_runner_->DeleteSoon(FROM_HERE, std::move(file_path_watcher_));
  }

  void SymbolicHotkeysChanged(bool error) {
    // Reset the SystemHotkeyMap whenever the symbolichotkeys.plist
    // changes. Note that there can be a significant delay between the
    // user making a change in System Preferences and System Preferences /
    // defaults writing those changes to disk. As a result, Chrome may
    // exhibit old behavior for some time (on the order of seconds) after
    // the user switches back to Chrome from System Preferences.
    system_hotkey_map_.reset();

    // When there's an error, the callback will never be called again.
    // If this happens, arrange to reinstate the callback the next time
    // LoadSystemHotkeyMap() gets called.
    if (error) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(file_path_watcher_));
      file_path_watcher_ = std::make_unique<base::FilePathWatcher>();
      configure_callback_ = true;
    }
  }

  void CallbackConfigured(bool success) {
    // Callback configuration failed so try again the next time the system
    // hotkey map is requested, and reread the map from disk in case it changed
    // and we missed it.
    if (!success) {
      system_hotkey_map_.reset();
      configure_callback_ = true;
    }
  }

  content::SystemHotkeyMap* GetSystemHotkeyMap() {
    using Callback = base::RepeatingCallback<void(bool)>;
    base::FilePath file_path = *SymbolicHotkeysPlistFilePath();

    if (!system_hotkey_map_.get()) {
      auto* hotkey_plist_url = base::mac::FilePathToNSURL(file_path);
      NSDictionary* dictionary =
          [NSDictionary dictionaryWithContentsOfURL:hotkey_plist_url
                                              error:NULL];

      system_hotkey_map_.reset(new content::SystemHotkeyMap());
      bool success = system_hotkey_map_->ParseDictionary(dictionary);
      UMA_HISTOGRAM_BOOLEAN("OSX.SystemHotkeyMap.LoadSuccess", success);
    }

    if (configure_callback_) {
      Callback on_change_callback(
          base::BindRepeating(&SystemHotkeyMapManager::SymbolicHotkeysChanged,
                              weak_ptr_factory_.GetWeakPtr()));

      task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &base::FilePathWatcher::Watch,
              base::Unretained(file_path_watcher_.get()),
              base::FilePath(file_path), base::FilePathWatcher::Type::kTrivial,
              base::BindRepeating(
                  [](base::SequencedTaskRunner* main_sequence,
                     const Callback& on_change_callback, const base::FilePath&,
                     bool error) {
                    main_sequence->PostTask(
                        FROM_HERE, base::BindOnce(on_change_callback, error));
                  },
                  base::RetainedRef(task_runner_),
                  std::move(on_change_callback))),
          base::BindOnce(&SystemHotkeyMapManager::CallbackConfigured,
                         base::Unretained(this)));

      configure_callback_ = false;
    }

    return system_hotkey_map_.get();
  }

  void WaitForEventsWithTimeout(base::TimeDelta timeout) {
    base::RunLoop run_loop;

    // Make sure we time out if we don't get notified.
    task_runner_->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
  }

 private:
  std::unique_ptr<content::SystemHotkeyMap> system_hotkey_map_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::FilePathWatcher> file_path_watcher_;
  bool configure_callback_ = true;

  base::WeakPtrFactory<SystemHotkeyMapManager> weak_ptr_factory_{this};
};

static base::NoDestructor<std::unique_ptr<SystemHotkeyMapManager>>
    system_hotkey_map_manager;

}  // namespace

namespace content {

// static
SystemHotkeyMapManager& GetSystemHotkeyManager() {
  if (system_hotkey_map_manager->get() == nullptr)
    system_hotkey_map_manager->reset(new SystemHotkeyMapManager);

  return *(system_hotkey_map_manager->get());
}

// static
SystemHotkeyMap* GetSystemHotkeyMap() {
  return GetSystemHotkeyManager().GetSystemHotkeyMap();
}

// static
void SetSystemHotkeyPlistPathForTesting(base::FilePath& file_path) {
  *SymbolicHotkeysPlistFilePath() = file_path;
}

// static
void WaitForEventsForTesting() {
  GetSystemHotkeyManager().WaitForEventsWithTimeout(base::Milliseconds(5));
}

}  // namespace content
