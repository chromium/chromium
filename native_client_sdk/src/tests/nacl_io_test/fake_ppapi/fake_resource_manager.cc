// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_resource_manager.h"
#include "gtest/gtest.h"
#include "sdk_util/auto_lock.h"

FakeResourceManager::FakeResourceManager() : next_handle_(1) {}

FakeResourceManager::~FakeResourceManager() {
  // The ref counts for all resources should be zero.
  for (ResourceMap::iterator iter = resource_map_.begin();
       iter != resource_map_.end();
       ++iter) {
    const FakeResourceTracker* resource_tracker = iter->second;
    EXPECT_EQ(0, resource_tracker->ref_count()) << "Leaked resource "
                                                << resource_tracker->classname()
                                                << "(" << iter->first
                                                << "), created at "
                                                << resource_tracker->file()
                                                << ":"
                                                << resource_tracker->line();
    delete resource_tracker;
  }
}

PP_Resource FakeResourceManager::Create(FakeResource* resource,
                                        const char* classname,
                                        const char* file,
                                        int line) {
  AUTO_LOCK(lock_);
  PP_Resource handle = next_handle_++;
  FakeResourceTracker* resource_tracker =
      new FakeResourceTracker(resource, classname, file, line);
  std::pair<ResourceMap::iterator, bool> result =
      resource_map_.insert(ResourceMap::value_type(handle, resource_tracker));
  EXPECT_TRUE(result.second);
  result.first->second->AddRef();
  return handle;
}

void FakeResourceManager::AddRef(PP_Resource handle) {
  AUTO_LOCK(lock_);
  ResourceMap::iterator iter = resource_map_.find(handle);
  ASSERT_NE(resource_map_.end(), iter) << "AddRefing unknown resource "
                                       << handle;

  FakeResourceTracker* resource_tracker = iter->second;
  EXPECT_LT(0, resource_tracker->ref_count()) << "AddRefing freed resource "
                                              << resource_tracker->classname()
                                              << "(" << handle
                                              << "), created at "
                                              << resource_tracker->file() << ":"
                                              << resource_tracker->line();
  resource_tracker->AddRef();
}

void FakeResourceManager::Release(PP_Resource handle) {
  if (handle == 0)
    return;

  sdk_util::AutoLock lock(lock_);
  ResourceMap::iterator iter = resource_map_.find(handle);
  ASSERT_NE(resource_map_.end(), iter) << "Releasing unknown resource "
                                       << handle;

  FakeResourceTracker* resource_tracker = iter->second;
  EXPECT_LT(0, resource_tracker->ref_count()) << "Releasing freed resource "
                                              << resource_tracker->classname()
                                              << "(" << handle
                                              << "), created at "
                                              << resource_tracker->file() << ":"
                                              << resource_tracker->line();
  resource_tracker->Release();
  // It's OK to access the tracker when its refcount is zero; it doesn't
  // actually destroy the object until the manager is destroyed.
  if (resource_tracker->ref_count() == 0) {
    // Remove the resource from this tracker.
    FakeResource* resource = resource_tracker->Pass();
    // Release the lock before we call Destroy; resources can call
    // FakeResourceManager::Release(), which will deadlock if we are already
    // holding the lock.
    lock.Unlock();

    resource->Destroy();
    delete resource;
  }
}

FakeResourceTracker* FakeResourceManager::Get(PP_Resource handle,
                                              bool not_found_ok) {
  AUTO_LOCK(lock_);
  ResourceMap::iterator iter = resource_map_.find(handle);
  if (iter == resource_map_.end()) {
    if (!not_found_ok) {
      // Can't use FAIL() because it tries to return void.
      EXPECT_TRUE(false) << "Trying to get resource " << handle
                         << " that doesn't exist!";
    }

    return NULL;
  }

  FakeResourceTracker* resource_tracker = iter->second;
  EXPECT_LT(0, resource_tracker->ref_count()) << "Accessing freed resource "
                                              << resource_tracker->classname()
                                              << "(" << handle
                                              << "), created at "
                                              << resource_tracker->file() << ":"
                                              << resource_tracker->line();

  return iter->second;
}

FakeResourceTracker::FakeResourceTracker(FakeResource* resource,
                                         const char* classname,
                                         const char* file,
                                         int line)
    : resource_(resource),
      classname_(classname),
      file_(file),
      line_(line),
      ref_count_(0) {}

FakeResourceTracker::~FakeResourceTracker() { delete resource_; }

bool FakeResourceTracker::CheckType(const char* other_classname) const {
  if (strcmp(other_classname, classname_) != 0) {
    // Repeat the expectation, just to print out a nice error message before we
    // crash. :)
    EXPECT_STREQ(classname_, other_classname);
    return false;
  }

  return true;
}
