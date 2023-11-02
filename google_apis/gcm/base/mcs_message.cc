// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/mcs_message.h"

#include <utility>

#include "base/check_op.h"
#include "google_apis/gcm/base/mcs_util.h"

namespace gcm {

MCSMessage::Core::Core() {}

MCSMessage::Core::Core(uint8_t tag,
                       const google::protobuf::MessageLite& protobuf) {
  std::unique_ptr<google::protobuf::MessageLite> owned_protobuf(protobuf.New());
  owned_protobuf->CheckTypeAndMergeFrom(protobuf);
  protobuf_ = std::move(owned_protobuf);
}

MCSMessage::Core::Core(
    uint8_t tag,
    std::unique_ptr<const google::protobuf::MessageLite> protobuf) {
  protobuf_ = std::move(protobuf);
}

MCSMessage::Core::~Core() {}

const google::protobuf::MessageLite& MCSMessage::Core::Get() const {
  return *protobuf_;
}

MCSMessage::MCSMessage() : tag_(0), size_(0) {}

MCSMessage::MCSMessage(const google::protobuf::MessageLite& protobuf)
  : tag_(GetMCSProtoTag(protobuf)),
    size_(protobuf.ByteSize()),
    core_(new Core(tag_, protobuf)) {
}

MCSMessage::MCSMessage(uint8_t tag,
                       const google::protobuf::MessageLite& protobuf)
    : tag_(tag), size_(protobuf.ByteSize()), core_(new Core(tag_, protobuf)) {
  DCHECK_EQ(tag, GetMCSProtoTag(protobuf));
}

MCSMessage::MCSMessage(
    uint8_t tag,
    std::unique_ptr<const google::protobuf::MessageLite> protobuf)
    : tag_(tag),
      size_(protobuf->ByteSize()),
      core_(new Core(tag_, std::move(protobuf))) {
  DCHECK_EQ(tag, GetMCSProtoTag(core_->Get()));
}

MCSMessage::MCSMessage(const MCSMessage& other) = default;

MCSMessage::~MCSMessage() {
}

bool MCSMessage::IsValid() const {
  return core_.get() != nullptr;
}

std::string MCSMessage::SerializeAsString() const {
  return core_->Get().SerializeAsString();
}

const google::protobuf::MessageLite& MCSMessage::GetProtobuf() const {
  return core_->Get();
}

std::unique_ptr<google::protobuf::MessageLite> MCSMessage::CloneProtobuf()
    const {
  std::unique_ptr<google::protobuf::MessageLite> clone(GetProtobuf().New());
  clone->CheckTypeAndMergeFrom(GetProtobuf());
  return clone;
}

}  // namespace gcm
