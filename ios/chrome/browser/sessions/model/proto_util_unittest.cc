// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/proto_util.h"

#include <optional>

#include "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#include "testing/platform_test.h"

using ProtoUtilTest = PlatformTest;

namespace {

// Helper function to create an OpenerStorage.
ios::proto::OpenerStorage CreateOpenerStorage(int index, int navigation_index) {
  ios::proto::OpenerStorage storage;
  storage.set_index(index);
  storage.set_navigation_index(navigation_index);
  return storage;
}

// Helper function to create a WebStateListItemStorage.
ios::proto::WebStateListItemStorage CreateWebStateListItemStorage(
    int identifier,
    std::optional<ios::proto::OpenerStorage> opener = std::nullopt) {
  ios::proto::WebStateListItemStorage storage;
  storage.set_identifier(identifier);
  if (opener) {
    *storage.mutable_opener() = *opener;
  }
  return storage;
}

// Helper function to create a WebStateListStorage.
ios::proto::WebStateListStorage CreateWebStateListStorage(
    int active_index,
    int pinned_item_count,
    std::initializer_list<ios::proto::WebStateListItemStorage> items) {
  ios::proto::WebStateListStorage storage;
  storage.set_active_index(active_index);
  storage.set_pinned_item_count(pinned_item_count);
  for (const ios::proto::WebStateListItemStorage& item : items) {
    *storage.add_items() = item;
  }
  return storage;
}

}  // namespace

// Tests the equality (and inequality) operator of `OpenerStorage`.
TEST_F(ProtoUtilTest, OpenerStorage_Equality) {
  // Check that default initialized objects are equals.
  EXPECT_EQ(ios::proto::OpenerStorage{}, ios::proto::OpenerStorage{});

  // Check that objects are distinct if `index` is different.
  EXPECT_NE(CreateOpenerStorage(1, 0), CreateOpenerStorage(2, 0));

  // Check that objects are distinct if `navigation_index` is different.
  EXPECT_NE(CreateOpenerStorage(0, 1), CreateOpenerStorage(0, 2));

  // Check that objects are equal if they have the same values.
  EXPECT_EQ(CreateOpenerStorage(1, 1), CreateOpenerStorage(1, 1));
}

// Tests the equality (and inequality) operator of `WebStateListItemStorage`.
TEST_F(ProtoUtilTest, WebStateListItemStorage_Equality) {
  // Check that default initialized objects are equals.
  EXPECT_EQ(ios::proto::WebStateListItemStorage{},
            ios::proto::WebStateListItemStorage{});

  // Check that objects are distinct if `identifier` is different.
  EXPECT_NE(CreateWebStateListItemStorage(1), CreateWebStateListItemStorage(2));

  // Check that objects are distinct if `opener` is different.
  EXPECT_NE(CreateWebStateListItemStorage(1),
            CreateWebStateListItemStorage(1, CreateOpenerStorage(1, 2)));

  // Check that objects are equal if one has an empty opener while the
  // other has no opener (i.e. absence of opener is considered as having
  // a default opener).
  EXPECT_EQ(CreateWebStateListItemStorage(1),
            CreateWebStateListItemStorage(1, ios::proto::OpenerStorage{}));

  // Check that objects are equal if they have the same values.
  EXPECT_EQ(CreateWebStateListItemStorage(1, CreateOpenerStorage(1, 2)),
            CreateWebStateListItemStorage(1, CreateOpenerStorage(1, 2)));
}

// Tests the equality (and inequality) operator of `WebStateListStorage`.
TEST_F(ProtoUtilTest, WebStateListStorage_Equality) {
  // Check that default initialized objects are equals.
  EXPECT_EQ(ios::proto::WebStateListStorage{},
            ios::proto::WebStateListStorage{});

  // Check objects are distinct if `active_index` is different.
  EXPECT_NE(CreateWebStateListStorage(1, 0, {}),
            CreateWebStateListStorage(2, 0, {}));

  // Check objects are distinct if `pinned_item_count` is different.
  EXPECT_NE(CreateWebStateListStorage(1, 1, {}),
            CreateWebStateListStorage(1, 2, {}));

  // Check objects are distinct if `items` list are of different size.
  EXPECT_NE(
      CreateWebStateListStorage(1, 1, {}),
      CreateWebStateListStorage(1, 1, {ios::proto::WebStateListItemStorage{}}));

  // Check objects are distinct if some item in `items` are different.
  EXPECT_NE(
      CreateWebStateListStorage(1, 1, {CreateWebStateListItemStorage(1)}),
      CreateWebStateListStorage(1, 1, {CreateWebStateListItemStorage(2)}));

  // Check that objects are equal if they have the same values.
  EXPECT_EQ(
      CreateWebStateListStorage(1, 1, {CreateWebStateListItemStorage(2)}),
      CreateWebStateListStorage(1, 1, {CreateWebStateListItemStorage(2)}));
}
