// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include <algorithm>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/android/media_common_android.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/android/media_url_interceptor.h"
#include "media/base/timestamp_constants.h"
#include "net/storage_access_api/status.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/MediaPlayerBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

namespace {

enum UMAExitStatus {
  UMA_EXIT_SUCCESS = 0,
  UMA_EXIT_ERROR,
  UMA_EXIT_STATUS_MAX = UMA_EXIT_ERROR,
};

const double kDefaultVolume = 1.0;

const char kWatchTimeHistogram[] = "Media.Android.MediaPlayerWatchTime";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WatchTimeType {
  kNonHls = 0,
  kHlsAudioOnly = 1,
  kHlsVideo = 2,
  kMaxValue = kHlsVideo,
};

void RecordWatchTimeUMA(bool is_hls, bool has_video) {
  WatchTimeType type = WatchTimeType::kNonHls;
  if (is_hls) {
    if (!has_video) {
      type = WatchTimeType::kHlsAudioOnly;
    } else {
      type = WatchTimeType::kHlsVideo;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(kWatchTimeHistogram, type);
}

}  // namespace

MediaPlayerBridge::MediaPlayerBridge(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    const std::string& user_agent,
    bool hide_url_log,
    Client* client,
    bool allow_credentials,
    bool is_hls,
    const base::flat_map<std::string, std::string> headers)
    : prepared_(false),
      playback_completed_(false),
      pending_play_(false),
      should_seek_on_prepare_(false),
      url_(url),
      site_for_cookies_(site_for_cookies),
      top_frame_origin_(top_frame_origin),
      storage_access_api_status_(storage_access_api_status),
      pending_retrieve_cookies_(false),
      should_prepare_on_retrieved_cookies_(false),
      user_agent_(user_agent),
      hide_url_log_(hide_url_log),
      width_(0),
      height_(0),
      can_seek_forward_(true),
      can_seek_backward_(true),
      volume_(kDefaultVolume),
      allow_credentials_(allow_credentials),
      is_active_(false),
      has_error_(false),
      has_ever_started_(false),
      is_hls_(is_hls),
      watch_timer_(base::BindRepeating(&MediaPlayerBridge::OnWatchTimerTick,
                                       base::Unretained(this)),
                   base::BindRepeating(&MediaPlayerBridge::GetCurrentTime,
                                       base::Unretained(this))),
      headers_(std::move(headers)),
      client_(client) {
  listener_ = std::make_unique<MediaPlayerListener>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      weak_factory_.GetWeakPtr());
}

MediaPlayerBridge::~MediaPlayerBridge() {
  if (!j_media_player_bridge_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    CHECK(env);
    Java_MediaPlayerBridge_destroy(env, j_media_player_bridge_);
  }
  Release();

  if (has_ever_started_) {
    UMA_HISTOGRAM_ENUMERATION("Media.Android.MediaPlayerSuccess",
                              has_error_ ? UMA_EXIT_ERROR : UMA_EXIT_SUCCESS,
                              UMA_EXIT_STATUS_MAX + 1);
  }
}

void MediaPlayerBridge::Initialize() {
  cookies_.clear();
  CHECK(!url_.SchemeIsBlob());
  CHECK(!url_.SchemeIsFileSystem());

  if (allow_credentials_ && !url_.SchemeIsFile()) {
    media::MediaResourceGetter* resource_getter =
        client_->GetMediaResourceGetter();

    pending_retrieve_cookies_ = true;
    resource_getter->GetCookies(
        url_, site_for_cookies_, top_frame_origin_, storage_access_api_status_,
        base::BindOnce(&MediaPlayerBridge::OnCookiesRetrieved,
                       weak_factory_.GetWeakPtr()));
  }
}

void MediaPlayerBridge::CreateJavaMediaPlayerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_player_bridge_.Reset(Java_MediaPlayerBridge_create(
      env, reinterpret_cast<intptr_t>(this)));

  UpdateVolumeInternal();

  AttachListener(j_media_player_bridge_);
}

void MediaPlayerBridge::PropagateDuration(base::TimeDelta duration) {
  duration_ = duration;
  client_->OnMediaDurationChanged(duration_);
}

void MediaPlayerBridge::SetVideoSurface(gl::ScopedJavaSurface surface) {
  surface_ = std::move(surface);

  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setSurface(env, j_media_player_bridge_,
                                    surface_.j_surface());
}

void MediaPlayerBridge::SetPlaybackRate(double playback_rate) {
  if (!prepared_) {
    pending_playback_rate_ = playback_rate;
    return;
  }

  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setPlaybackRate(env, j_media_player_bridge_,
                                         playback_rate);
}

void MediaPlayerBridge::Prepare() {
  DCHECK(j_media_player_bridge_.is_null());
  CHECK(!url_.SchemeIsBlob());
  CHECK(!url_.SchemeIsFileSystem());

  CreateJavaMediaPlayerBridge();

  SetDataSource(url_.spec());
}

void MediaPlayerBridge::SetDataSource(const std::string& url) {
  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  int fd;
  int64_t offset;
  int64_t size;
  if (InterceptMediaUrl(url, &fd, &offset, &size)) {
    if (!Java_MediaPlayerBridge_setDataSourceFromFd(env, j_media_player_bridge_,
                                                    fd, offset, size)) {
      OnMediaError(MEDIA_ERROR_FORMAT);
      return;
    }

    if (!Java_MediaPlayerBridge_prepareAsync(env, j_media_player_bridge_))
      OnMediaError(MEDIA_ERROR_FORMAT);

    return;
  }

  // Create a Java String for the URL.
  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url);

  const std::string data_uri_prefix("data:");
  if (base::StartsWith(url, data_uri_prefix, base::CompareCase::SENSITIVE)) {
    if (!Java_MediaPlayerBridge_setDataUriDataSource(
            env, j_media_player_bridge_, j_url_string)) {
      OnMediaError(MEDIA_ERROR_FORMAT);
    }
    return;
  }

  // Cookies may not have been retrieved yet, delay prepare until they are
  // retrieved.
  if (pending_retrieve_cookies_) {
    should_prepare_on_retrieved_cookies_ = true;
    return;
  }
  SetDataSourceInternal();
}

void MediaPlayerBridge::SetDataSourceInternal() {
  DCHECK(!pending_retrieve_cookies_);

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jstring> j_cookies =
      ConvertUTF8ToJavaString(env, cookies_);
  ScopedJavaLocalRef<jstring> j_user_agent =
      ConvertUTF8ToJavaString(env, user_agent_);
  ScopedJavaLocalRef<jstring> j_url_string =
      ConvertUTF8ToJavaString(env, url_.spec());

  jclass hashMapClass = env->FindClass("java/util/HashMap");
  jmethodID hashMapConstructor =
      env->GetMethodID(hashMapClass, "<init>", "()V");
  jobject javaHashMap = env->NewObject(hashMapClass, hashMapConstructor);
  for (const auto& entry : headers_) {
    jstring key = env->NewStringUTF(entry.first.c_str());
    jstring value = env->NewStringUTF(entry.second.c_str());
    jmethodID putMethod = env->GetMethodID(
        hashMapClass, "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    env->CallObjectMethod(javaHashMap, putMethod, key, value);
    env->DeleteLocalRef(key);
    env->DeleteLocalRef(value);
  }
  base::android::ScopedJavaLocalRef<jobject> scoped_hash_map(env, javaHashMap);
  if (!Java_MediaPlayerBridge_setDataSource(
          env, j_media_player_bridge_, j_url_string, j_cookies, j_user_agent,
          hide_url_log_, scoped_hash_map)) {
    OnMediaError(MEDIA_ERROR_FORMAT);
    return;
  }

  if (!Java_MediaPlayerBridge_prepareAsync(env, j_media_player_bridge_))
    OnMediaError(MEDIA_ERROR_FORMAT);
}

bool MediaPlayerBridge::InterceptMediaUrl(const std::string& url,
                                          int* fd,
                                          int64_t* offset,
                                          int64_t* size) {
  // Sentinel value to check whether the output arguments have been set.
  const int kUnsetValue = -1;

  *fd = kUnsetValue;
  *offset = kUnsetValue;
  *size = kUnsetValue;
  media::MediaUrlInterceptor* url_interceptor =
      client_->GetMediaUrlInterceptor();
  if (url_interceptor && url_interceptor->Intercept(url, fd, offset, size)) {
    DCHECK_NE(kUnsetValue, *fd);
    DCHECK_NE(kUnsetValue, *offset);
    DCHECK_NE(kUnsetValue, *size);
    return true;
  }
  return false;
}

void MediaPlayerBridge::OnDidSetDataUriDataSource(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean success) {
  if (!success) {
    OnMediaError(MEDIA_ERROR_FORMAT);
    return;
  }

  if (!Java_MediaPlayerBridge_prepareAsync(env, j_media_player_bridge_))
    OnMediaError(MEDIA_ERROR_FORMAT);
}

void MediaPlayerBridge::OnCookiesRetrieved(const std::string& cookies) {
  cookies_ = cookies;
  pending_retrieve_cookies_ = false;
  client_->GetMediaResourceGetter()->GetAuthCredentials(
      url_, base::BindOnce(&MediaPlayerBridge::OnAuthCredentialsRetrieved,
                           weak_factory_.GetWeakPtr()));

  if (should_prepare_on_retrieved_cookies_) {
    SetDataSourceInternal();
    should_prepare_on_retrieved_cookies_ = false;
  }
}

void MediaPlayerBridge::OnAuthCredentialsRetrieved(
    const std::u16string& username,
    const std::u16string& password) {
  GURL::ReplacementsW replacements;
  if (!username.empty()) {
    replacements.SetUsernameStr(username);
    if (!password.empty())
      replacements.SetPasswordStr(password);
    url_ = url_.ReplaceComponents(replacements);
  }
}

void MediaPlayerBridge::Start() {
  // A second Start() call after an error is considered another attempt for UMA
  // and causes UMA reporting.
  if (has_ever_started_ && has_error_) {
    UMA_HISTOGRAM_ENUMERATION("Media.Android.MediaPlayerSuccess",
                              UMA_EXIT_ERROR, UMA_EXIT_STATUS_MAX + 1);
  }

  has_ever_started_ = true;
  has_error_ = false;
  is_active_ = true;

  if (j_media_player_bridge_.is_null()) {
    pending_play_ = true;
    Prepare();
  } else {
    if (prepared_)
      StartInternal();
    else
      pending_play_ = true;
  }
}

void MediaPlayerBridge::Pause() {
  if (j_media_player_bridge_.is_null()) {
    pending_play_ = false;
  } else {
    if (prepared_ && IsPlaying())
      PauseInternal();
    else
      pending_play_ = false;
  }

  is_active_ = false;
}

bool MediaPlayerBridge::IsPlaying() {
  DCHECK(prepared_);
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  jboolean result =
      Java_MediaPlayerBridge_isPlaying(env, j_media_player_bridge_);
  return result;
}

void MediaPlayerBridge::SeekTo(base::TimeDelta timestamp) {
  // Record the time to seek when OnMediaPrepared() is called.
  pending_seek_ = timestamp;
  should_seek_on_prepare_ = true;

  if (prepared_)
    SeekInternal(timestamp);
}

base::TimeDelta MediaPlayerBridge::GetCurrentTime() {
  if (!prepared_)
    return pending_seek_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::Milliseconds(
      Java_MediaPlayerBridge_getCurrentPosition(env, j_media_player_bridge_));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  DCHECK(prepared_);

  JNIEnv* env = base::android::AttachCurrentThread();
  const int duration_ms =
      Java_MediaPlayerBridge_getDuration(env, j_media_player_bridge_);
  return duration_ms < 0 ? media::kInfiniteDuration
                         : base::Milliseconds(duration_ms);
}

void MediaPlayerBridge::Release() {
  watch_timer_.Stop();
  is_active_ = false;

  if (j_media_player_bridge_.is_null())
    return;

  if (prepared_) {
    pending_seek_ = GetCurrentTime();
    should_seek_on_prepare_ = true;
  }

  prepared_ = false;
  pending_play_ = false;
  SetVideoSurface(gl::ScopedJavaSurface());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_release(env, j_media_player_bridge_);
  j_media_player_bridge_.Reset();
  DetachListener();
}

void MediaPlayerBridge::SetVolume(double volume) {
  volume_ = std::clamp(volume, 0.0, 1.0);
  UpdateVolumeInternal();
}

void MediaPlayerBridge::UpdateVolumeInternal() {
  if (j_media_player_bridge_.is_null()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setVolume(env, j_media_player_bridge_, volume_);
}

void MediaPlayerBridge::OnVideoSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
  client_->OnVideoSizeChanged(width, height);
}

void MediaPlayerBridge::OnMediaError(int error_type) {
  // Gather errors for UMA only in the active state.
  // The MEDIA_ERROR_INVALID_CODE is reported by MediaPlayerListener.java in
  // the situations that are considered normal, and is ignored by upper level.
  if (is_active_ && error_type != MEDIA_ERROR_INVALID_CODE)
    has_error_ = true;

  // Do not propagate MEDIA_ERROR_SERVER_DIED. If it happens in the active state
  // we want the playback to stall. It can be recovered by pressing the Play
  // button again.
  if (error_type == MEDIA_ERROR_SERVER_DIED)
    error_type = MEDIA_ERROR_INVALID_CODE;

  client_->OnError(error_type);
}

void MediaPlayerBridge::OnPlaybackComplete() {
  if (!playback_completed_) {
    playback_completed_ = true;
    client_->OnPlaybackComplete();
  }
}

void MediaPlayerBridge::OnMediaPrepared() {
  if (j_media_player_bridge_.is_null())
    return;

  prepared_ = true;
  PropagateDuration(GetDuration());

  UpdateAllowedOperations();

  // If media player was recovered from a saved state, consume all the pending
  // events.
  if (should_seek_on_prepare_) {
    SeekInternal(pending_seek_);
    pending_seek_ = base::Milliseconds(0);
    should_seek_on_prepare_ = false;
  }

  if (!surface_.IsEmpty())
    SetVideoSurface(std::move(surface_));

  if (pending_play_) {
    StartInternal();
    pending_play_ = false;
  }

  if (pending_playback_rate_) {
    SetPlaybackRate(pending_playback_rate_.value());
    pending_playback_rate_.reset();
  }
}

ScopedJavaLocalRef<jobject> MediaPlayerBridge::GetAllowedOperations() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  return Java_MediaPlayerBridge_getAllowedOperations(env,
                                                     j_media_player_bridge_);
}

void MediaPlayerBridge::AttachListener(const JavaRef<jobject>& j_media_player) {
  listener_->CreateMediaPlayerListener(j_media_player);
}

void MediaPlayerBridge::DetachListener() {
  listener_->ReleaseMediaPlayerListenerResources();
}

base::WeakPtr<MediaPlayerBridge> MediaPlayerBridge::WeakPtrForUIThread() {
  return weak_factory_.GetWeakPtr();
}

void MediaPlayerBridge::UpdateAllowedOperations() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jobject> allowedOperations = GetAllowedOperations();

  can_seek_forward_ =
      Java_AllowedOperations_canSeekForward(env, allowedOperations);
  can_seek_backward_ =
      Java_AllowedOperations_canSeekBackward(env, allowedOperations);
}

void MediaPlayerBridge::StartInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_start(env, j_media_player_bridge_);
  watch_timer_.Start();
}

void MediaPlayerBridge::PauseInternal() {
  watch_timer_.Stop();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_pause(env, j_media_player_bridge_);
}

void MediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  base::TimeDelta current_time = GetCurrentTime();

  // Seeking on content like live streams may cause the media player to
  // get stuck in an error state.
  if (time < current_time && !can_seek_backward_)
    return;

  if (time >= current_time && !can_seek_forward_)
    return;

  if (time > duration_)
    time = duration_;

  // Seeking to an invalid position may cause media player to stuck in an
  // error state.
  if (time.is_negative()) {
    DCHECK_EQ(-1.0, time.InMillisecondsF());
    return;
  }

  playback_completed_ = false;

  // Note: we do not want to count changes in media time due to seeks as watch
  // time, but tracking pending seeks is not completely trivial. Instead seeks
  // larger than kWatchTimeReportingInterval * 2 will be discarded by the sanity
  // checks and shorter seeks will be counted.
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  int time_msec = static_cast<int>(time.InMilliseconds());
  Java_MediaPlayerBridge_seekTo(env, j_media_player_bridge_, time_msec);
}

GURL MediaPlayerBridge::GetUrl() {
  return url_;
}

const net::SiteForCookies& MediaPlayerBridge::GetSiteForCookies() {
  return site_for_cookies_;
}

void MediaPlayerBridge::OnWatchTimerTick() {
  RecordWatchTimeUMA(is_hls_, height_ > 0);
}

}  // namespace media
