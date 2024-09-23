// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/buffer_source.h"

#include "base/ranges/algorithm.h"
#include "components/zucchini/algorithm.h"

namespace zucchini {

BufferSource::BufferSource(const ConstBufferView& buffer)
    : ConstBufferView(buffer) {}

BufferSource::BufferSource(const ConstBufferView& buffer, size_type offset)
    : ConstBufferView(buffer) {
  Skip(offset);
}

bool BufferSource::Skip(size_type n) {
  if (n > Remaining()) {
    remove_prefix(Remaining());
    return false;
  }
  remove_prefix(n);
  return true;
}

bool BufferSource::CheckNextBytes(std::initializer_list<uint8_t> bytes) const {
  if (Remaining() < bytes.size()) {
    return false;
  }
  return base::ranges::mismatch(bytes, *this).first == bytes.end();
}

bool BufferSource::ConsumeBytes(std::initializer_list<uint8_t> bytes) {
  if (!CheckNextBytes(bytes)) {
    return false;
  }
  remove_prefix(bytes.size());
  return true;
}

bool BufferSource::GetRegion(size_type count, ConstBufferView* buffer) {
  DCHECK_NE(begin(), nullptr);
  if (Remaining() < count) {
    return false;
  }
  *buffer = ConstBufferView(begin(), count);
  remove_prefix(count);
  return true;
}

// [0aaaaaaa] => 00000000'00000000'00000000'0aaaaaaa
// [1aaaaaaa 0bbbbbbb] => 00000000'00000000'00bbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 0ccccccc] => 00000000'000ccccc'ccbbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 1ccccccc 0ddddddd] => 0000dddd'dddccccc'ccbbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 1ccccccc 1ddddddd 0???eeee]
//     => eeeedddd'dddccccc'ccbbbbbb'baaaaaaa
// Note that "???" is discarded. Meanwhile, 1???eeee is invalid.
bool BufferSource::GetUleb128(uint32_t* ret) {
  int shift_lim =
      static_cast<int>(std::min<size_type>(kMaxLeb128Size, size())) * 7;
  const_iterator cur = cbegin();
  uint32_t value = 0U;
  for (int shift = 0; shift < shift_lim; shift += 7, ++cur) {
    uint32_t b = *cur;
    // When |shift == 28|, |(b & 0x7F) << shift| discards the "???" bits.
    value |= static_cast<uint32_t>(b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *ret = value;
      seek(cur + 1);
      return true;
    }
  }
  return false;
}

// [0Saaaaaa] => SSSSSSSS'SSSSSSSS'SSSSSSSS'SSaaaaaa
// [1aaaaaaa 0Sbbbbbb] => SSSSSSSS'SSSSSSSS'SSSbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 0Scccccc] => SSSSSSSS'SSSScccc'ccbbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 1ccccccc 0Sdddddd] => SSSSSddd'dddccccc'ccbbbbbb'baaaaaaa
// [1aaaaaaa 1bbbbbbb 1ccccccc 1ddddddd 0???Seee]
//     => Seeedddd'dddccccc'ccbbbbbb'baaaaaaa
// Note that "???" is discarded. Meanwhile, 1???eeee is invalid.
bool BufferSource::GetSleb128(int32_t* ret) {
  int shift_lim =
      static_cast<int>(std::min<size_type>(kMaxLeb128Size, size())) * 7;
  const_iterator cur = cbegin();
  int32_t value = 0;
  for (int shift = 0; shift < shift_lim; shift += 7, ++cur) {
    uint32_t b = *cur;
    // When |shift == 28|, |(b & 0x7F) << shift| discards the "???" bits.
    value |= static_cast<int32_t>(static_cast<uint32_t>(b & 0x7F) << shift);
    if (!(b & 0x80)) {
      *ret = (shift == 28) ? value : SignExtend(shift + 6, value);
      seek(cur + 1);
      return true;
    }
  }
  return false;
}

bool BufferSource::SkipLeb128() {
  int lim = static_cast<int>(std::min<size_type>(kMaxLeb128Size, size()));
  const_iterator cur = cbegin();
  for (int i = 0; i < lim; ++i, ++cur) {
    if (!(*cur & 0x80)) {
      seek(cur + 1);
      return true;
    }
  }
  return false;
}

}  // namespace zucchini
