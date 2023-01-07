// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_TYPE_ENFORCEMENT_H_
#define MEDIA_BASE_MEDIA_LOG_TYPE_ENFORCEMENT_H_

#include "media/base/media_serializers.h"

namespace media {

namespace internal {
enum class UnmatchableType {};
}  // namespace internal

// Forward declare the enums.
enum class MediaLogProperty;
enum class MediaLogEvent;

// Allow only specific types for an individual property.
template <MediaLogProperty PROP, typename T>
struct MediaLogPropertyTypeSupport {};

// Allow only specific types for an individual event.
// However unlike Property, T is not required, so we default it to some
// unmatchable type that will never be passed as an argument accidentally.
template <MediaLogEvent EVENT, typename T = internal::UnmatchableType>
struct MediaLogEventTypeSupport {};

// Lets us define the supported type in a single line in media_log_properties.h.
#define MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(PROPERTY, TYPE)                 \
  template <>                                                            \
  struct MediaLogPropertyTypeSupport<MediaLogProperty::PROPERTY, TYPE> { \
    static base::Value Convert(const TYPE& type) {                       \
      return MediaSerialize<TYPE>(type);                                 \
    }                                                                    \
  }

#define MEDIA_LOG_EVENT_NAMED_DATA(EVENT, TYPE, DISPLAY)                 \
  template <>                                                            \
  struct MediaLogEventTypeSupport<MediaLogEvent::EVENT, TYPE> {          \
    static void AddExtraData(base::Value::Dict* params, const TYPE& t) { \
      DCHECK(params);                                                    \
      params->Set(DISPLAY, MediaSerialize<TYPE>(t));                     \
    }                                                                    \
    static std::string TypeName() { return #EVENT; }                     \
  }

#define MEDIA_LOG_EVENT_NAMED_DATA_OP(EVENT, TYPE, DISPLAY, OP)          \
  template <>                                                            \
  struct MediaLogEventTypeSupport<MediaLogEvent::EVENT, TYPE> {          \
    static void AddExtraData(base::Value::Dict* params, const TYPE& t) { \
      DCHECK(params);                                                    \
      params->Set(DISPLAY, MediaSerialize<TYPE>(OP(t)));                 \
    }                                                                    \
    static std::string TypeName() { return #EVENT; }                     \
  }

// Specifically do not create the Convert or DisplayName methods
#define MEDIA_LOG_EVENT_TYPELESS(EVENT)                    \
  template <>                                              \
  struct MediaLogEventTypeSupport<MediaLogEvent::EVENT> {  \
    static std::string TypeName() { return #EVENT; }       \
    static void AddExtraData(base::Value::Dict* params) {} \
  }

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_TYPE_ENFORCEMENT_H_
