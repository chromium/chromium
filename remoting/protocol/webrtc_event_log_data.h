// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_EVENT_LOG_DATA_H_
#define REMOTING_PROTOCOL_WEBRTC_EVENT_LOG_DATA_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"

namespace remoting::protocol {

// A data store which records the most recent RTC event log data. This is
// written to by an RTCEventLogOutput instance, which is owned by the
// PeerConnection (but the data itself is owned by the caller). In order to
// keep the most recent events (but keep the amount of memory-allocations
// reasonable), the data is maintained as a circular list of sections.
class WebrtcEventLogData {
 public:
  using LogSection = std::vector<std::uint8_t>;

  WebrtcEventLogData();
  ~WebrtcEventLogData();

  WebrtcEventLogData(const WebrtcEventLogData&) = delete;
  WebrtcEventLogData& operator=(const WebrtcEventLogData&) = delete;

  // Allows tests to lower the storage limits.
  void SetMaxSectionSizeForTest(int max_section_size);
  void SetMaxSectionsForTest(int max_sections);

  // Transfers the current log data to the caller and clears the data. This can
  // safely be used without stopping the event logging.
  base::circular_deque<LogSection> TakeLogData();

  // Adds a new log event. This may discard old events, and may invalidate
  // iterators even if nothing is discarded. Note that the log_event may
  // theoretically exceed the maximum size of a section (though this should
  // never occur because these are packet-headers which are small, no bigger
  // than RTCP packets). If that ever happens, the log_event will be stored in
  // a new section anyway - the buffer's reserved capacity may be exceeded and
  // re-allocation may occur.
  void Write(std::string_view log_event);

  // Removes all event data, so the instance can be reused.
  void Clear();

 private:
  // Returns true if a new section must be created to store the event.
  bool NeedNewSection(size_t log_event_size) const;

  // Appends a new section of zero size to the end of the list, removing the
  // oldest one if necessary. On return, the section at the end (the list's
  // "back") will be empty, ready to accept the new data.
  void CreateNewSection();

  base::circular_deque<LogSection> sections_;

  // Value chosen to keep the memory-usage within reasonable limits, but also
  // allow for recording "most" sessions entirely.
  const int kMaxSections = 1000;
  int max_sections_ = kMaxSections;

  // A larger value will reduce memory-allocations at the cost of discarding a
  // larger chunk of the event log.
  const int kMaxSectionSize = 102400;  // 100K
  int max_section_size_ = kMaxSectionSize;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_EVENT_LOG_DATA_H_
