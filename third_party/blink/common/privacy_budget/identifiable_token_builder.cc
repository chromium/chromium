// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/hash/legacy_hash.h"

namespace blink {

IdentifiableTokenBuilder::IdentifiableTokenBuilder()
    : chaining_value_(kChainingValueSeed) {
  // Ensures that BlockBuffer iterators are random-access on all platforms.
  static_assert(
      std::is_same<std::random_access_iterator_tag,
                   std::iterator_traits<
                       BlockBuffer::iterator>::iterator_category>::value,
      "Iterator operations may not be constant time.");
}

IdentifiableTokenBuilder::IdentifiableTokenBuilder(
    const IdentifiableTokenBuilder& other) {
  partial_ = other.partial_;
  position_ = partial_.begin();
  std::advance(position_, other.PartialSize());
  chaining_value_ = other.chaining_value_;
}

IdentifiableTokenBuilder::IdentifiableTokenBuilder(ByteSpan buffer)
    : IdentifiableTokenBuilder() {
  AddBytes(buffer);
}

IdentifiableTokenBuilder& IdentifiableTokenBuilder::AddBytes(ByteSpan message) {
  DCHECK(position_ != partial_.end());
  // Phase 1:
  //    Slurp in as much of the message as necessary if there's a partial block
  //    already assembled. Copying is expensive, so |partial_| is only involved
  //    when there's some left over bytes from a prior round.
  if (partial_.begin() != position_ && !message.empty())
    message = SkimIntoPartial(message);

  if (message.empty())
    return *this;

  // Phase 2:
  //    Consume as many full blocks as possible from |message|.
  DCHECK(position_ == partial_.begin());
  while (message.size() >= kBlockSizeInBytes) {
    DigestBlock(message.first<kBlockSizeInBytes>());
    message = message.subspan(kBlockSizeInBytes);
  }
  if (message.empty())
    return *this;

  // Phase 3:
  //    Whatever remains is stuffed into the partial buffer.
  message = SkimIntoPartial(message);
  DCHECK(message.empty());
  return *this;
}

IdentifiableTokenBuilder& IdentifiableTokenBuilder::AddAtomic(ByteSpan buffer) {
  AlignPartialBuffer();
  AddValue(buffer.size_bytes());
  AddBytes(buffer);
  AlignPartialBuffer();
  return *this;
}

IdentifiableTokenBuilder::operator IdentifiableToken() const {
  return GetToken();
}

IdentifiableToken IdentifiableTokenBuilder::GetToken() const {
  if (position_ == partial_.begin())
    return chaining_value_;

  return IdentifiableToken(
      base::legacy::CityHash64WithSeed(GetPartialBlock(), chaining_value_));
}

IdentifiableTokenBuilder::ByteSpan IdentifiableTokenBuilder::SkimIntoPartial(
    ByteSpan message) {
  DCHECK(!message.empty() && position_ != partial_.end());
  const auto to_copy = std::min<size_t>(
      std::distance(position_, partial_.end()), message.size());
  position_ = std::copy_n(message.begin(), to_copy, position_);
  if (position_ == partial_.end())
    DigestBlock(TakeCompletedBlock());
  return message.subspan(to_copy);
}

void IdentifiableTokenBuilder::AlignPartialBuffer() {
  const auto padding_to_add =
      kBlockAlignment - (PartialSize() % kBlockAlignment);
  if (padding_to_add == kBlockAlignment)
    return;

  position_ = std::fill_n(position_, padding_to_add, 0);

  if (position_ == partial_.end())
    DigestBlock(TakeCompletedBlock());

  DCHECK(position_ != partial_.end());
  DCHECK(IsAligned());
}

void IdentifiableTokenBuilder::DigestBlock(ConstFullBlockSpan block) {
  // partial_ should've been flushed before calling this.
  DCHECK(position_ == partial_.begin());

  // The chaining value (initialized with the initialization vector
  // kChainingValueSeed) is only used for diffusion. There's no length padding
  // being done here since we aren't interested in second-preimage issues.
  //
  // There is a concern over hash flooding, but that's something the entire
  // study has more-or-less accepted for some metrics and is dealt with during
  // the analysis phase.
  chaining_value_ =
      base::legacy::CityHash64WithSeed(base::make_span(block), chaining_value_);
}

size_t IdentifiableTokenBuilder::PartialSize() const {
  return std::distance<BlockBuffer::const_iterator>(partial_.begin(),
                                                    position_);
}

IdentifiableTokenBuilder::ConstFullBlockSpan
IdentifiableTokenBuilder::TakeCompletedBlock() {
  DCHECK(position_ == partial_.end());
  auto buffer = base::make_span(partial_);
  position_ = partial_.begin();
  return buffer;
}

bool IdentifiableTokenBuilder::IsAligned() const {
  return PartialSize() % kBlockAlignment == 0;
}

IdentifiableTokenBuilder::ByteSpan IdentifiableTokenBuilder::GetPartialBlock()
    const {
  return ByteSpan(partial_).first(PartialSize());
}

}  // namespace blink
