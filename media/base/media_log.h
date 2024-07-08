// Copyright 2012 The Chromium Authors
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

#include "base/gtest_prod_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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

#if BUILDFLAG(IS_APPLE)
#include "base/apple/osstatus_logging.h"
#endif  // BUILDFLAG(IS_APPLE)

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

// Maximum limit for the total number of logs kept per renderer. At the time of
// writing, 512 events of the kind: { "property": value } together consume ~88kb
// of memory on linux.
#if BUILDFLAG(IS_ANDROID)
  static constexpr size_t kLogLimit = 128;
#else
  static constexpr size_t kLogLimit = 512;
#endif

  MediaLog(const MediaLog&) = delete;
  MediaLog& operator=(const MediaLog&) = delete;

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

  // Notify a non-ok Status. This method Should _not_ be given an OK status.
  template <typename T>
  void NotifyError(const TypedStatus<T>& status) {
    DCHECK(!status.is_ok());
    std::unique_ptr<MediaLogRecord> record =
        CreateRecord(MediaLogRecord::Type::kMediaStatus);
    base::Value serialized = MediaSerialize(status);
    DCHECK(serialized.is_dict());
    if (DCHECK_IS_ON() && DLOG_IS_ON(ERROR) && ShouldLogToDebugConsole()) {
      EmitConsoleErrorLog(serialized.GetDict().Clone());
    }
    record->params.Merge(std::move(serialized.GetDict()));
    AddLogRecord(std::move(record));
  }

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

  // Add a record to this log.  Inheritors should override AddLogRecordLocked to
  // do something. This needs to be public for MojoMediaLogService to use it.
  void AddLogRecord(std::unique_ptr<MediaLogRecord> event);

  // Provide a MediaLog which can have a separate lifetime from this one, but
  // still write to the same player's log.  It is not guaranteed that this will
  // log forever; it might start silently discarding log messages if the
  // original log is closed by whoever owns it.  However, it's safe to use it
  // even if this occurs, in the "won't crash" sense.
  virtual std::unique_ptr<MediaLog> Clone();

  // Can be used for stopping a MediaLog during a garbage-collected destruction
  // sequence.
  virtual void Stop();

  // Returns true if logs should be emitted to the console in debug mode. Some
  // subclasses will disable this.
  virtual bool ShouldLogToDebugConsole() const;

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
    record->params.Set(MediaLogPropertyKeyToString(P),
                       MediaLogPropertyTypeSupport<P, T>::Convert(value));
    return record;
  }
  template <MediaLogEvent E, typename... Opt>
  std::unique_ptr<MediaLogRecord> CreateEventRecord() {
    std::unique_ptr<MediaLogRecord> record(
        CreateRecord(MediaLogRecord::Type::kMediaEventTriggered));
    record->params.Set(MediaLog::kEventKey,
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

    ParentLogRecord(const ParentLogRecord&) = delete;
    ParentLogRecord& operator=(const ParentLogRecord&) = delete;

    // |lock_| protects the rest of this structure.
    base::Lock lock;

    // Original media log, or null.
    raw_ptr<MediaLog> media_log GUARDED_BY(lock) = nullptr;

   protected:
    friend class base::RefCountedThreadSafe<ParentLogRecord>;
    virtual ~ParentLogRecord();
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

  // Helper method for emitting error logs to console.
  void EmitConsoleErrorLog(base::Value::Dict status_dict);

  // The underlying media log.
  scoped_refptr<ParentLogRecord> parent_log_record_;
};

// Helper class to make it easier to use MediaLog like DVLOG().
class MEDIA_EXPORT LogHelper {
 public:
  LogHelper(MediaLogMessageLevel level,
            MediaLog* media_log,
            const char* file,
            int line,
            std::optional<logging::SystemErrorCode> code = std::nullopt);
  LogHelper(MediaLogMessageLevel level,
            const std::unique_ptr<MediaLog>& media_log,
            const char* file,
            int line,
            std::optional<logging::SystemErrorCode> code = std::nullopt);
  ~LogHelper();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  const int line_;
  const MediaLogMessageLevel level_;
  const raw_ptr<MediaLog> media_log_;
  const std::optional<logging::SystemErrorCode> code_;
  std::stringstream stream_;
};

// Provides a stringstream to collect a log entry to pass to the provided
// MediaLog at the requested level.
#if DCHECK_IS_ON()
#define MEDIA_PLOG(level, code, media_log)                               \
  media::LogHelper((media::MediaLogMessageLevel::k##level), (media_log), \
                   __FILE__, __LINE__, code)                             \
      .stream()
#define MEDIA_LOG(level, media_log)                                      \
  media::LogHelper((media::MediaLogMessageLevel::k##level), (media_log), \
                   __FILE__, __LINE__)                                   \
      .stream()
#else
#define MEDIA_LOG(level, media_log)                                      \
  media::LogHelper((media::MediaLogMessageLevel::k##level), (media_log), \
                   nullptr, 0)                                           \
      .stream()
#define MEDIA_PLOG(level, code, media_log)                               \
  media::LogHelper((media::MediaLogMessageLevel::k##level), (media_log), \
                   nullptr, 0, code)                                     \
      .stream()
#endif

#if BUILDFLAG(IS_APPLE)
// Prepends a description of an OSStatus to the log entry produced with
// `MEDIA_LOG`.
#define OSSTATUS_MEDIA_LOG(level, status, media_log) \
  MEDIA_LOG(level, media_log)                        \
      << logging::DescriptionFromOSStatus(status) << " (" << (status) << "): "
#endif  // BUILDFLAG(IS_APPLE)

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
