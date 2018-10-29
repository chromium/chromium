// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simulator/actor.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"

namespace quic {
namespace simulator {

Actor::Actor(Simulator* simulator, QuicString name)
    : simulator_(simulator),
      clock_(simulator->GetClock()),
      name_(std::move(name)) {
  simulator->AddActor(this);
}

Actor::~Actor() {}

void Actor::Schedule(QuicTime next_tick) {
  simulator_->Schedule(this, next_tick);
}

void Actor::Unschedule() {
  simulator_->Unschedule(this);
}

}  // namespace simulator
}  // namespace quic
