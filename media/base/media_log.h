// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/media_log_events.h"
#include "media/base/media_log_message_levels.h"
#include "media/base/media_log_properties.h"
#include "media/base/media_log_record.h"
#include "media/base/pipeline_status.h"
#include "url/gurl.h"

namespace media {

// Interface for media components to log to chrome://media-internals log.
//
// To provide a logging implementation, derive from MediaLog instead.
//
// Implementations only need to implement AddLogRecordLocked(), which must be
// thread safe in the sense that it may be called from multiple threads, though
// it will not be called concurrently.  See below for more details.
//
// Implementations should also call InvalidateLog during destruction, to signal
// to any child logs that the underlying log is no longer available.
class MEDIA_EXPORT MediaLog {
 public:
  static const char kEventKey[];
  static const char kStatusText[];

// Maximum limit for the total number of logs kept per renderer. At the time of
// writing, 512 events of the kind: { "property": value } together consume ~88kb
// of memory on linux.
#if defined(OS_ANDROID)
  static constexpr size_t kLogLimit = 128;
#else
  static constexpr size_t kLogLimit = 512;
#endif

  // Constructor is protected, see below.
  virtual ~MediaLog();

  // Report a log message at the specified log level.
  void AddMessage(MediaLogMessageLevel level, std::string message);

  // Typechecked property setter, since all properties must take values.
  // For example, MediaLogProperty::kResolution supports only gfx::Size as
  // an argument (see media_log_properties.h for this), so calling
  // media_log->SetProperty<MediaLogProperty::kResolution>(1);
  // would lead to a compile error, while
  // gfx::Size rect = {100, 100};
  // media_log->SetProperty<MediaLogProperty::kResolution>(rect);
  // is correct.
  template <MediaLogProperty P, typename T>
  void SetProperty(const T& value) {
    AddLogRecord(CreatePropertyRecord<P, T>(value));
  }

  // TODO(tmathmeyer) add the ability to report events with a separated
  // start and end time.
  // Send an event to the media log that may or may not have attached data.
  // For example, MediaLogEvent::kPlay takes no arguments, while
  // MediaLogEvent::kSeek takes a double as an argument, representing the time.
  // A proper way to add either of these events would be
  // media_log->AddEvent<MediaLogEvent::kPlay>();
  // media_log->AddEvent<MediaLogEvent::kSeek>(1.99);
  template <MediaLogEvent E, typename... T>
  void AddEvent(const T&... value) {
    std::unique_ptr<MediaLogRecord> record = CreateEventRecord<E, T...>();
    MediaLogEventTypeSupport<E, T...>::AddExtraData(&record->params, value...);
    AddLogRecord(std::move(record));
  }

  // TODO(tmathmeyer) replace with Status when that's ready.
  void NotifyError(PipelineStatus status);

  // Notify a non-ok Status. This method Should _not_ be given an OK status.
  void NotifyError(Status status);

  // Notify the media log that the player is destroyed. Some implementations
  // will want to change event handling based on this.
  void OnWebMediaPlayerDestroyed();

  // Returns a string usable as the contents of a MediaError.message.
  // This method returns an incomplete message if it is called before the
  // pertinent events for the error have been added to the log.
  // Note: The base class definition only produces empty messages. See
  // RenderMediaLog for where this method is meaningful.
  // Inheritors should override GetErrorMessageLocked().
  // TODO(tmathmeyer) Use a media::Status when that is ready.
  std::string GetErrorMessage();

  // Getter for |id_|. Used by MojoMediaLogService to construct MediaLogRecords
  // to log into this MediaLog. Also used in trace events to associate each
  // event with a specific media playback.
  int32_t id() const { return parent_log_record_->id; }

  // Add a record to this log.  Inheritors should override AddLogRecordLocked to
  // do something. This needs to be public for MojoMediaLogService to use it.
  void AddLogRecord(std::unique_ptr<MediaLogRecord> event);

  // Provide a MediaLog which can have a separate lifetime from this one, but
  // still write to the same player's log.  It is not guaranteed that this will
  // log forever; it might start silently discarding log messages if the
  // original log is closed by whoever owns it.  However, it's safe to use it
  // even if this occurs, in the "won't crash" sense.
  virtual std::unique_ptr<MediaLog> Clone();

 protected:
  // Ensures only subclasses and factories (e.g. Clone()) can create MediaLog.
  MediaLog();

  // Methods that may be overridden by inheritors.  All calls may arrive on any
  // thread, but will be synchronized with respect to any other *Locked calls on
  // any other thread, and with any parent log invalidation.
  //
  // Please see the documentation for the corresponding public methods.
  virtual void AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event);
  virtual void OnWebMediaPlayerDestroyedLocked();
  virtual std::string GetErrorMessageLocked();

  // MockMediaLog also needs to call this method.
  template <MediaLogProperty P, typename T>
  std::unique_ptr<MediaLogRecord> CreatePropertyRecord(const T& value) {
    auto record = CreateRecord(MediaLogRecord::Type::kMediaPropertyChange);
    record->params.SetKey(MediaLogPropertyKeyToString(P),
                          MediaLogPropertyTypeSupport<P, T>::Convert(value));
    return record;
  }
  template <MediaLogEvent E, typename... Opt>
  std::unique_ptr<MediaLogRecord> CreateEventRecord() {
    std::unique_ptr<MediaLogRecord> record(
        CreateRecord(MediaLogRecord::Type::kMediaEventTriggered));
    record->params.SetString(MediaLog::kEventKey,
                             MediaLogEventTypeSupport<E, Opt...>::TypeName());
    return record;
  }

  // Notify all child logs that they should stop working.  This should be called
  // to guarantee that no further calls into AddLogRecord should be allowed.
  // Further, since calls into this log may happen on any thread, it's important
  // to call this while the log is still in working order.  For example, calling
  // it immediately during destruction is a good idea.
  void InvalidateLog();

  struct ParentLogRecord : base::RefCountedThreadSafe<ParentLogRecord> {
    explicit ParentLogRecord(MediaLog* log);

    // A unique (to this process) id for this MediaLog.
    int32_t id;

    // |lock_| protects the rest of this structure.
    base::Lock lock;

    // Original media log, or null.
    MediaLog* media_log GUARDED_BY(lock) = nullptr;

   protected:
    friend class base::RefCountedThreadSafe<ParentLogRecord>;
    virtual ~ParentLogRecord();

    DISALLOW_COPY_AND_ASSIGN(ParentLogRecord);
  };

 private:
  // Allows MediaLogTest to construct MediaLog directly for testing.
  friend class MediaLogTest;
  FRIEND_TEST_ALL_PREFIXES(MediaLogTest, EventsAreForwarded);
  FRIEND_TEST_ALL_PREFIXES(MediaLogTest, EventsAreNotForwardedAfterInvalidate);

  // Use |parent_log_record| instead of making a new one.
  explicit MediaLog(scoped_refptr<ParentLogRecord> parent_log_record);

  // Helper methods to create events and their parameters.
  std::unique_ptr<MediaLogRecord> CreateRecord(MediaLogRecord::Type type);

  // The underlying media log.
  scoped_refptr<ParentLogRecord> parent_log_record_;

  DISALLOW_COPY_AND_ASSIGN(MediaLog);
};

// Helper class to make it easier to use MediaLog like DVLOG().
class MEDIA_EXPORT LogHelper {
 public:
  LogHelper(MediaLogMessageLevel level, MediaLog* media_log);
  LogHelper(MediaLogMessageLevel level,
            const std::unique_ptr<MediaLog>& media_log);
  ~LogHelper();

  std::ostream& stream() { return stream_; }

 private:
  const MediaLogMessageLevel level_;
  MediaLog* const media_log_;
  std::stringstream stream_;
};

// Provides a stringstream to collect a log entry to pass to the provided
// MediaLog at the requested level.
#define MEDIA_LOG(level, media_log) \
  LogHelper((MediaLogMessageLevel::k##level), (media_log)).stream()

// Logs only while |count| < |max|, increments |count| for each log, and warns
// in the log if |count| has just reached |max|.
// Multiple short-circuit evaluations are involved in this macro:
// 1) LAZY_STREAM avoids wasteful MEDIA_LOG and evaluation of subsequent stream
//    arguments if |count| is >= |max|, and
// 2) the |condition| given to LAZY_STREAM itself short-circuits to prevent
//    incrementing |count| beyond |max|.
// Note that LAZY_STREAM guarantees exactly one evaluation of |condition|, so
// |count| will be incremented at most once each time this macro runs.
// The "|| true" portion of |condition| lets logging occur correctly when
// |count| < |max| and |count|++ is 0.
// TODO(wolenetz,chcunningham): Consider using a helper class instead of a macro
// to improve readability.
#define LIMITED_MEDIA_LOG(level, media_log, count, max)                       \
  LAZY_STREAM(MEDIA_LOG(level, media_log),                                    \
              (count) < (max) && ((count)++ || true))                         \
      << (((count) == (max)) ? "(Log limit reached. Further similar entries " \
                               "may be suppressed): "                         \
                             : "")

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_H_
