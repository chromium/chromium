// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/synthesized_history_entry_data.h"

#import "base/strings/utf_string_conversions.h"

namespace web {

SynthesizedHistoryEntryData::SynthesizedHistoryEntryData() {}
SynthesizedHistoryEntryData::~SynthesizedHistoryEntryData() {}

NSData* SynthesizedHistoryEntryData::AsNSData() {
  // A unique sequence number, in a way similar to WebKit, see:
  // https://github.com/WebKit/WebKit/blob/730fec84/Source/WebCore/history/HistoryItem.cpp#L48
  static uint64_t next =
      static_cast<uint64_t>([[NSDate date] timeIntervalSince1970] * 1000);

  struct __attribute__((packed)) DataPart1 {
    uint64_t header;
    uint32_t version;
    uint32_t padding;
    uint64_t child_count;
    uint64_t document_seq_number;
    uint64_t document_state_vector_size;
    uint32_t form_content_type;
    uint32_t has_form_data;
    uint64_t item_seq_number;
  };

  struct __attribute__((packed)) DataPart2 {
    uint32_t scroll_position_x;
    uint32_t scroll_position_y;
    uint32_t page_scale_factor;
    uint32_t has_state_object;
    uint32_t frame_state_target;
    unsigned __int128 exposed_content_rect;
    unsigned __int128 unobscured_content_rect;
    uint64_t minimum_layout_size_in_scroll_view_coordinates;
    uint64_t content_size;
    uint8_t scale_is_initial;
  };

  // This is based off the current size of all the elements (mostly zero) being
  // written to the buffer.
  std::size_t reserved_size = sizeof(DataPart1) + sizeof(DataPart2);
  DCHECK(reserved_size == 125);

  DataPart1 data_part_1 = {};
  data_part_1.version = 2;
  data_part_1.form_content_type = 0xFFFFFFFF;
  data_part_1.document_seq_number = ++next;
  data_part_1.item_seq_number = ++next;

  DataPart2 data_part_2 = {};
  data_part_2.scale_is_initial = 1;
  data_part_2.frame_state_target = 0xFFFFFFFF;

  // The only variable sized element is currently the referrer_
  uint64_t referrer_spec_len = referrer_.spec().length();
  referrer_spec_len += referrer_spec_len % 2;  // round up to even.
  reserved_size += (referrer_spec_len) ? (16 + (referrer_spec_len * 2)) : 4;

  buffer_.reserve(reserved_size);
  PushBack(data_part_1);
  PushBackGURL(referrer_);
  PushBack(data_part_2);
  DCHECK(buffer_.size() == reserved_size);
  return [NSData dataWithBytes:buffer_.data() length:buffer_.size()];
}

void SynthesizedHistoryEntryData::PushBack(const uint8_t* data, size_t size) {
  buffer_.insert(buffer_.end(), data, data + size);
}

void SynthesizedHistoryEntryData::PushBackGURL(const GURL& url) {
  if (url.spec().length() == 0) {
    static constexpr uint32_t kEmptyString = 0xFFFFFFFF;
    PushBack(kEmptyString);
    return;
  }

  size_t original_size = buffer_.size();
  // URLs are saved in the following format:
  // u64: length in characters
  // u64: length in bytes (2 * length in characters)
  // u16 * length: spec encoded in UTF-16
  // padding: enough padding for u32 alignment
  //
  // This mean that if length % 2 == 1, then padding will
  // be two bytes, otherwise there is no need for padding.
  std::u16string url_u16 = base::UTF8ToUTF16(url.spec());
  PushBack(url_u16.size());
  PushBack(url_u16.size() * (sizeof(char16_t) / sizeof(char)));
  PushBack(reinterpret_cast<const uint8_t*>(url_u16.data()),
           url_u16.size() * (sizeof(char16_t) / sizeof(char)));

  static constexpr uint16_t kAlignment = 0;
  if (url_u16.size() % 2 != 0) {
    PushBack(kAlignment);
  }
  DCHECK((buffer_.size() - original_size) % 2 == 0);
}

}  // namespace web
