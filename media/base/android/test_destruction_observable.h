// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_
#define MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"

namespace media {

class DestructionObserver;

// DestructionObservable is a base class for testing that lets you set
// expectations on its lifetime without keeping a reference to it. Each
// observable can create a single DestructionObserver pointing to it.
class DestructionObservable {
 public:
  DestructionObservable();

  DestructionObservable(const DestructionObservable&) = delete;
  DestructionObservable& operator=(const DestructionObservable&) = delete;

  virtual ~DestructionObservable();
  std::unique_ptr<DestructionObserver> CreateDestructionObserver();

  base::ScopedClosureRunner destruction_cb;
};

// DestructionObserver lets you set expectations about the destruction of an
// observable.
class DestructionObserver {
 public:
  DestructionObserver(DestructionObservable* observable);

  DestructionObserver(const DestructionObserver&) = delete;
  DestructionObserver& operator=(const DestructionObserver&) = delete;

  virtual ~DestructionObserver();

  void VerifyAndClearExpectations();

  // Sets an expectation that the observable is destructed before the observer.
  // Asserts that it's not already destructed. Cancels the previous expectation.
  void ExpectDestruction();

  // Sets an expectation that the observable is not destructed before the
  // observer. Asserts that it's not already destructed. Cancels the previous
  // expectation.
  void DoNotAllowDestruction();

  // Return if the object has been destroyed.
  bool destructed() const { return destructed_; }

 private:
  void VerifyExpectations();
  void OnObservableDestructed();

  // Whether the observable has been destructed.
  bool destructed_;

  // Whether to expect destruction. Unset if there is no expectation.
  std::optional<bool> expect_destruction_;

  base::WeakPtrFactory<DestructionObserver> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_TEST_DESTRUCTION_OBSERVABLE_H_
