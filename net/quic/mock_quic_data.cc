// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_quic_data.h"

#include "net/base/hex_utils.h"
#include "net/socket/socket_test_util.h"

namespace net::test {

MockQuicData::MockQuicData(quic::ParsedQuicVersion version)
    : printer_(version) {}

MockQuicData::~MockQuicData() = default;

void MockQuicData::AddConnect(IoMode mode, int rv) {
  connect_ = std::make_unique<MockConnect>(mode, rv);
}

void MockQuicData::AddConnect(MockConnectCompleter* completer) {
  connect_ = std::make_unique<MockConnect>(completer);
}

void MockQuicData::AddRead(IoMode mode,
                           std::unique_ptr<quic::QuicReceivedPacket> packet) {
  reads_.emplace_back(mode, packet->data(), packet->length(),
                      sequence_number_++,
                      static_cast<uint8_t>(packet->ecn_codepoint()));
  packets_.push_back(std::move(packet));
}
void MockQuicData::AddRead(IoMode mode,
                           std::unique_ptr<quic::QuicEncryptedPacket> packet) {
  reads_.emplace_back(mode, packet->data(), packet->length(),
                      sequence_number_++, /*tos=*/0);
  packets_.push_back(std::move(packet));
}
void MockQuicData::AddRead(IoMode mode, int rv) {
  reads_.emplace_back(mode, rv, sequence_number_++);
}

void MockQuicData::AddReadPause() {
  // Add a sentinel value that indicates a read pause.
  AddRead(ASYNC, ERR_IO_PENDING);
}

void MockQuicData::AddReadPauseForever() {
  // Add a sentinel value that indicates a read pause forever.
  AddRead(SYNCHRONOUS, ERR_IO_PENDING);
}

void MockQuicData::AddWrite(IoMode mode,
                            std::unique_ptr<quic::QuicEncryptedPacket> packet) {
  writes_.emplace_back(mode, packet->data(), packet->length(),
                       sequence_number_++);
  packets_.push_back(std::move(packet));
}

void MockQuicData::AddWrite(IoMode mode, int rv) {
  writes_.emplace_back(mode, rv, sequence_number_++);
}

void MockQuicData::AddWrite(IoMode mode,
                            int rv,
                            std::unique_ptr<quic::QuicEncryptedPacket> packet) {
  writes_.emplace_back(mode, rv, sequence_number_++);
  packets_.push_back(std::move(packet));
}

void MockQuicData::AddWritePause() {
  // Add a sentinel value that indicates a write pause.
  AddWrite(ASYNC, ERR_IO_PENDING);
}

void MockQuicData::AddSocketDataToFactory(MockClientSocketFactory* factory) {
  factory->AddSocketDataProvider(InitializeAndGetSequencedSocketData());
}

bool MockQuicData::AllReadDataConsumed() {
  return socket_data_->AllReadDataConsumed();
}

bool MockQuicData::AllWriteDataConsumed() {
  return socket_data_->AllWriteDataConsumed();
}

void MockQuicData::ExpectAllReadDataConsumed() {
  socket_data_->ExpectAllReadDataConsumed();
}

void MockQuicData::ExpectAllWriteDataConsumed() {
  socket_data_->ExpectAllWriteDataConsumed();
}

void MockQuicData::Resume() {
  socket_data_->Resume();
}

SequencedSocketData* MockQuicData::InitializeAndGetSequencedSocketData() {
  socket_data_ = std::make_unique<SequencedSocketData>(reads_, writes_);
  socket_data_->set_printer(&printer_);
  if (connect_ != nullptr)
    socket_data_->set_connect_data(*connect_);

  return socket_data_.get();
}

SequencedSocketData* MockQuicData::GetSequencedSocketData() {
  return socket_data_.get();
}

}  // namespace net::test
