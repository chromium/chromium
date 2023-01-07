// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/special_storage_policy.h"

#include "base/observer_list.h"

namespace storage {

SpecialStoragePolicy::Observer::~Observer() = default;

SpecialStoragePolicy::SpecialStoragePolicy() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SpecialStoragePolicy::~SpecialStoragePolicy() = default;

void SpecialStoragePolicy::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SpecialStoragePolicy::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SpecialStoragePolicy::NotifyGranted(const url::Origin& origin,
                                         int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<SpecialStoragePolicy> protect(this);
  for (auto& observer : observers_)
    observer.OnGranted(origin, change_flags);
  NotifyPolicyChanged();
}

void SpecialStoragePolicy::NotifyRevoked(const url::Origin& origin,
                                         int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<SpecialStoragePolicy> protect(this);
  for (auto& observer : observers_)
    observer.OnRevoked(origin, change_flags);
  NotifyPolicyChanged();
}

void SpecialStoragePolicy::NotifyCleared() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<SpecialStoragePolicy> protect(this);
  for (auto& observer : observers_)
    observer.OnCleared();
  NotifyPolicyChanged();
}

void SpecialStoragePolicy::NotifyPolicyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<SpecialStoragePolicy> protect(this);
  for (auto& observer : observers_)
    observer.OnPolicyChanged();
}

}  // namespace storage
