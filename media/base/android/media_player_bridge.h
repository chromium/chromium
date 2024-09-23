// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_

#include <jni.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_player_listener.h"
#include "media/base/media_export.h"
#include "media/base/simple_watch_timer.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

class MediaResourceGetter;
class MediaUrlInterceptor;

// This class serves as a bridge between the native code and Android MediaPlayer
// Java class. For more information on Android MediaPlayer, check
// http://developer.android.com/reference/android/media/MediaPlayer.html
// The actual Android MediaPlayer instance is created lazily when Start(),
// Pause(), SeekTo() gets called. As a result, media information may not
// be available until one of those operations is performed. After that, we
// will cache those information in case the mediaplayer gets released.
// The class uses the corresponding MediaPlayerBridge Java class to talk to
// the Android MediaPlayer instance.
class MEDIA_EXPORT MediaPlayerBridge {
 public:
  class Client {
   public:
    // Returns a pointer to the MediaResourceGetter object.
    virtual MediaResourceGetter* GetMediaResourceGetter() = 0;

    // Returns a pointer to the MediaUrlInterceptor object or null.
    virtual MediaUrlInterceptor* GetMediaUrlInterceptor() = 0;

    // Called when media duration is first detected or changes.
    virtual void OnMediaDurationChanged(base::TimeDelta duration) = 0;

    // Called when playback completed.
    virtual void OnPlaybackComplete() = 0;

    // Called when error happens.
    virtual void OnError(int error) = 0;

    // Called when video size has changed.
    virtual void OnVideoSizeChanged(int width, int height) = 0;
  };

  // Error types for MediaErrorCB.
  enum MediaErrorType {
    MEDIA_ERROR_FORMAT,
    MEDIA_ERROR_DECODE,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
    MEDIA_ERROR_SERVER_DIED,
  };

  // Construct a MediaPlayerBridge object. This object needs to call `client`'s
  // GetMediaResourceGetter() before decoding the media stream. This allows
  // `client` to track unused resources and free them when needed.
  // MediaPlayerBridge also forwards Android MediaPlayer callbacks to
  // the `client` when needed.
  MediaPlayerBridge(const GURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const url::Origin& top_frame_origin,
                    net::StorageAccessApiStatus storage_access_api_status,
                    const std::string& user_agent,
                    bool hide_url_log,
                    Client* client,
                    bool allow_credentials,
                    bool is_hls,
                    const base::flat_map<std::string, std::string> headers);

  MediaPlayerBridge(const MediaPlayerBridge&) = delete;
  MediaPlayerBridge& operator=(const MediaPlayerBridge&) = delete;

  virtual ~MediaPlayerBridge();

  // Initialize this object and extract the metadata from the media.
  void Initialize();

  // Methods to partially expose the underlying MediaPlayer.
  void SetVideoSurface(gl::ScopedJavaSurface surface);
  void SetPlaybackRate(double playback_rate);
  void Pause();
  void SeekTo(base::TimeDelta timestamp);
  base::TimeDelta GetCurrentTime();

  // Starts media playback.
  // The first call to this method will call Prepare() and create the underlying
  // MediaPlayer for the first time.
  void Start();

  // The media URL given to the underlying MediaPlayer.
  GURL GetUrl();

  // The site whose cookies should be given to the MediaPlayer if needed.
  const net::SiteForCookies& GetSiteForCookies();

  // Set the player volume, and take effect immediately.
  // The volume should be between 0.0 and 1.0.
  void SetVolume(double volume);

  void OnDidSetDataUriDataSource(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean success);

 private:
  friend class MediaPlayerListener;
  friend class MediaPlayerBridgeTest;

  // Releases the resources such as the underlying MediaPlayer and
  // MediaPlayerListener.
  void Release();

  base::TimeDelta GetDuration();
  void PropagateDuration(base::TimeDelta time);
  bool IsPlaying();

  // Prepare the player for playback, asynchronously. When succeeds,
  // OnMediaPrepared() will be called. Otherwise, OnMediaError() will
  // be called with an error type.
  void Prepare();

  // MediaPlayerListener callbacks.
  void OnVideoSizeChanged(int width, int height);
  void OnMediaError(int error_type);
  void OnPlaybackComplete();
  void OnMediaPrepared();

  // Create the corresponding Java class instance.
  void CreateJavaMediaPlayerBridge();

  // Get allowed operations from the player.
  base::android::ScopedJavaLocalRef<jobject> GetAllowedOperations();

  // Attach/Detaches `listener_` for listening to all the media events. If
  // `j_media_player` is NULL, `listener_` only listens to the system media
  // events. Otherwise, it also listens to the events from `j_media_player`.
  void AttachListener(const base::android::JavaRef<jobject>& j_media_player);
  void DetachListener();

  // Set the data source for the media player.
  void SetDataSource(const std::string& url);
  void SetDataSourceInternal();

  // Functions that implements media player control.
  void StartInternal();
  void PauseInternal();

  // Calls Java MediaPlayerBridge's seekTo method, or no-ops if the operation
  // is not allowed (based off of `can_seek_forward_` and `can_seek_backward_`).
  void SeekInternal(base::TimeDelta time);

  // Update allowed operations from the player.
  void UpdateAllowedOperations();

  // Callback function passed to `resource_getter_`. Called when the cookies
  // are retrieved.
  void OnCookiesRetrieved(const std::string& cookies);

  // Callback function passed to `resource_getter_`. Called when the auth
  // credentials are retrieved.
  void OnAuthCredentialsRetrieved(const std::u16string& username,
                                  const std::u16string& password);

  // Extract the media metadata from a url, asynchronously.
  // OnMediaMetadataExtracted() will be called when this call finishes.
  void ExtractMediaMetadata(const std::string& url);
  void OnMediaMetadataExtracted(base::TimeDelta duration,
                                int width,
                                int height,
                                bool success);

  // Returns true if a MediaUrlInterceptor registered by the embedder has
  // intercepted the url.
  bool InterceptMediaUrl(const std::string& url,
                         int* fd,
                         int64_t* offset,
                         int64_t* size);

  // Sets the underlying MediaPlayer's volume.
  void UpdateVolumeInternal();

  void OnWatchTimerTick();

  base::WeakPtr<MediaPlayerBridge> WeakPtrForUIThread();

  // Whether the player is prepared for playback.
  bool prepared_;

  // Whether the player completed playback.
  bool playback_completed_;

  // Pending play event while player is preparing.
  bool pending_play_;

  // Pending seek time while player is preparing.
  base::TimeDelta pending_seek_;

  // Whether a seek should be performed after preparing.
  bool should_seek_on_prepare_;

  // Url for playback.
  GURL url_;

  // Used to determine if cookies are accessed in a third-party context.
  net::SiteForCookies site_for_cookies_;

  // Used to check for cookie content settings.
  url::Origin top_frame_origin_;

  // Used when determining if first-party cookies may be accessible in a
  // third-party context.
  net::StorageAccessApiStatus storage_access_api_status_;

  // Waiting to retrieve cookies for `url_`.
  bool pending_retrieve_cookies_;

  // Whether to prepare after cookies retrieved.
  bool should_prepare_on_retrieved_cookies_;

  // User agent string to be used for media player.
  const std::string user_agent_;

  // Hide url log from media player.
  bool hide_url_log_;

  // Stats about the media.
  base::TimeDelta duration_;
  int width_;
  int height_;

  bool can_seek_forward_;
  bool can_seek_backward_;

  // The player volume. Should be between 0.0 and 1.0.
  double volume_;

  // Cookies for `url_`.
  std::string cookies_;

  // The surface object currently owned by the player.
  gl::ScopedJavaSurface surface_;

  // Java MediaPlayerBridge instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_player_bridge_;

  // Whether user credentials are allowed to be passed.
  bool allow_credentials_;

  // Whether the preparation for playback or the playback is currently going on.
  // This flag is set in Start() and cleared in Pause() and Release(). Used for
  // UMA reporting only.
  bool is_active_;

  // Whether there has been any errors in the active state.
  bool has_error_;

  // The flag is set if Start() has been called at least once.
  bool has_ever_started_;

  // State for watch time reporting.
  bool is_hls_;
  SimpleWatchTimer watch_timer_;

  // HTTP Request Headers
  base::flat_map<std::string, std::string> headers_;

  // A reference to the owner of `this`.
  raw_ptr<Client> client_;

  // Listener object that listens to all the media player events.
  std::unique_ptr<MediaPlayerListener> listener_;

  // Pending playback rate while player is preparing.
  std::optional<double> pending_playback_rate_;

  // Weak pointer passed to `listener_` for callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaPlayerBridge> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
