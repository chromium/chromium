// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/test_destruction_observable.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

DestructionObservable::DestructionObservable() = default;

DestructionObservable::~DestructionObservable() = default;

std::unique_ptr<DestructionObserver>
DestructionObservable::CreateDestructionObserver() {
  return std::make_unique<DestructionObserver>(this);
}

DestructionObserver::DestructionObserver(DestructionObservable* observable)
    : destructed_(false) {
  // Only one observer is allowed.
  DCHECK(!observable->destruction_cb.Release());
  observable->destruction_cb.ReplaceClosure(
      base::BindOnce(&DestructionObserver::OnObservableDestructed,
                     weak_factory_.GetWeakPtr()));
}

DestructionObserver::~DestructionObserver() {
  VerifyExpectations();
}

void DestructionObserver::VerifyAndClearExpectations() {
  VerifyExpectations();
  expect_destruction_.reset();
}

void DestructionObserver::VerifyExpectations() {
  if (!expect_destruction_.has_value())
    return;
  if (*expect_destruction_)
    ASSERT_TRUE(destructed_) << "Expected the observable to be destructed.";
  else
    ASSERT_FALSE(destructed_) << "Expected the observable to not be destructed";
}

void DestructionObserver::OnObservableDestructed() {
  destructed_ = true;
  VerifyExpectations();
}

void DestructionObserver::ExpectDestruction() {
  ASSERT_FALSE(destructed_);
  expect_destruction_ = true;
}

void DestructionObserver::DoNotAllowDestruction() {
  ASSERT_FALSE(destructed_);
  expect_destruction_ = false;
}

}  // namespace media
