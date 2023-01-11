// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_

#include "base/functional/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif

#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class BrowserContext;
class StoragePartition;

class CONTENT_EXPORT BackgroundSyncLauncher {
 public:
  static BackgroundSyncLauncher* Get();

  BackgroundSyncLauncher(const BackgroundSyncLauncher&) = delete;
  BackgroundSyncLauncher& operator=(const BackgroundSyncLauncher&) = delete;

  static base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      BrowserContext* browser_context);
#if BUILDFLAG(IS_ANDROID)
  static void FireBackgroundSyncEvents(
      BrowserContext* browser_context,
      blink::mojom::BackgroundSyncType sync_type,
      const base::android::JavaParamRef<jobject>& j_runnable);
  base::TimeDelta TimeSinceLastBrowserWakeUpForPeriodicSync();
#endif

 private:
  friend struct base::LazyInstanceTraitsBase<BackgroundSyncLauncher>;
  friend class BackgroundSyncLauncherTest;
  friend class BackgroundSyncManagerTest;

  // Constructor and destructor marked private to enforce singleton.
  BackgroundSyncLauncher();
  ~BackgroundSyncLauncher();

  base::TimeDelta GetSoonestWakeupDeltaImpl(
      blink::mojom::BackgroundSyncType sync_type,
      BrowserContext* browser_context);
#if BUILDFLAG(IS_ANDROID)
  void FireBackgroundSyncEventsImpl(
      BrowserContext* browser_context,
      blink::mojom::BackgroundSyncType sync_type,
      const base::android::JavaParamRef<jobject>& j_runnable);
#endif
  void GetSoonestWakeupDeltaForStoragePartition(
      blink::mojom::BackgroundSyncType sync_type,
      StoragePartition* storage_partition);
  void SendSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::OnceCallback<void(base::TimeDelta)> callback);

  // Getter and setter for |soonest_wakeup_delta_one_shot_|
  // or |soonest_wakeup_delta_periodic_| based on |sync_type|.
  void SetGlobalSoonestWakeupDelta(blink::mojom::BackgroundSyncType sync_type,
                                   base::TimeDelta set_to);
  base::TimeDelta GetGlobalSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type);

  base::TimeDelta soonest_wakeup_delta_one_shot_ = base::TimeDelta::Max();
  base::TimeDelta soonest_wakeup_delta_periodic_ = base::TimeDelta::Max();
  base::Time last_browser_wakeup_for_periodic_sync_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_
