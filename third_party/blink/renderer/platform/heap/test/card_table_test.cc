// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"

namespace blink {

namespace {

class BaseObject : public GarbageCollected<BaseObject> {
 public:
  size_t CardNumber() const;
  void Trace(Visitor*) const {}
};

}  // namespace

class CardTableTest : public TestSupportingGC {
 public:
  static constexpr size_t kCardSize = NormalPage::CardTable::kCardSize;

  CardTableTest() { ClearOutOldGarbage(); }

  static void CheckObjects(const std::vector<BaseObject*>& objects,
                           const NormalPage& page) {
    page.IterateCardTable([&objects](HeapObjectHeader* header) {
      const BaseObject* object =
          reinterpret_cast<BaseObject*>(header->Payload());
      auto it = std::find(objects.begin(), objects.end(), object);
      EXPECT_NE(it, objects.end());
    });
  }

  static void MarkCardForObject(BaseObject* object) {
    NormalPage* page = static_cast<NormalPage*>(PageFromObject(object));
    page->MarkCard(reinterpret_cast<Address>(object));
  }

  static bool IsCardMarked(const NormalPage& page, size_t card_number) {
    return page.card_table_.IsMarked(card_number);
  }

  static size_t ObjectsInCard(const NormalPage& page, size_t card_number) {
    const NormalPage::CardTable& cards = page.card_table_;

    size_t objects = 0;
    Address card_begin =
        RoundToBlinkPageStart(page.GetAddress()) + (card_number * kCardSize);
    const Address card_end = card_begin + kCardSize;
    if (card_number == cards.begin().index) {
      // First card is misaligned due to padding.
      card_begin = page.Payload();
    }

    page.ArenaForNormalPage()->MakeConsistentForGC();

    page.IterateOnCard(
        [card_begin, card_end, &objects](HeapObjectHeader* header) {
          const Address header_address = reinterpret_cast<Address>(header);
          if (header_address < card_begin) {
            const Address next_header_address = header_address + header->size();
            EXPECT_GT(next_header_address, card_begin);
          } else {
            objects++;
            EXPECT_LT(header_address, card_end);
          }
        },
        card_number);

    return objects;
  }

  static size_t MarkedObjects(const NormalPage& page) {
    size_t objects = 0;
    page.ArenaForNormalPage()->MakeConsistentForGC();
    page.IterateCardTable([&objects](HeapObjectHeader*) { ++objects; });
    return objects;
  }

  static void ClearCardTable(NormalPage& page) { page.card_table_.Clear(); }
};

namespace {

size_t BaseObject::CardNumber() const {
  return (reinterpret_cast<uintptr_t>(this) & kBlinkPageOffsetMask) /
         CardTableTest::kCardSize;
}

template <size_t Size>
class Object : public BaseObject {
 private:
  uint8_t array[Size];
};

}  // namespace

TEST_F(CardTableTest, Empty) {
  BaseObject* obj = MakeGarbageCollected<BaseObject>();
  EXPECT_EQ(0u, MarkedObjects(*static_cast<NormalPage*>(PageFromObject(obj))));
}

TEST_F(CardTableTest, SingleObjectOnFirstCard) {
  BaseObject* obj = MakeGarbageCollected<BaseObject>();
  MarkCardForObject(obj);

  const NormalPage& page = *static_cast<NormalPage*>(PageFromObject(obj));
  const size_t card_number = obj->CardNumber();
  EXPECT_TRUE(IsCardMarked(page, card_number));

  const size_t objects = ObjectsInCard(page, card_number);
  EXPECT_EQ(1u, objects);
}

TEST_F(CardTableTest, SingleObjectOnSecondCard) {
  MakeGarbageCollected<Object<kCardSize>>();
  BaseObject* obj = MakeGarbageCollected<Object<kCardSize>>();
  MarkCardForObject(obj);

  const NormalPage& page = *static_cast<NormalPage*>(PageFromObject(obj));
  const size_t card_number = obj->CardNumber();
  EXPECT_TRUE(IsCardMarked(page, card_number));

  const size_t objects = ObjectsInCard(page, card_number);
  EXPECT_EQ(1u, objects);
}

TEST_F(CardTableTest, TwoObjectsOnSecondCard) {
  static constexpr size_t kHalfCardSize = kCardSize / 2;
  MakeGarbageCollected<Object<kHalfCardSize>>();
  MakeGarbageCollected<Object<kHalfCardSize>>();
  // The card on which 'obj' resides is guaranteed to have two objects, either
  // the previously allocated one or the following one.
  BaseObject* obj = MakeGarbageCollected<Object<kHalfCardSize>>();
  MakeGarbageCollected<Object<kHalfCardSize>>();
  MarkCardForObject(obj);

  const NormalPage& page = *static_cast<NormalPage*>(PageFromObject(obj));
  const size_t card_number = obj->CardNumber();
  EXPECT_TRUE(IsCardMarked(page, card_number));

  const size_t objects = ObjectsInCard(page, card_number);
  EXPECT_EQ(2u, objects);
}

TEST_F(CardTableTest, Clear) {
  MakeGarbageCollected<Object<kCardSize>>();
  MakeGarbageCollected<Object<kCardSize / 2>>();
  BaseObject* obj = MakeGarbageCollected<Object<kCardSize / 2>>();
  MarkCardForObject(obj);

  NormalPage& page = *static_cast<NormalPage*>(PageFromObject(obj));
  ClearCardTable(page);

  const size_t card_number = obj->CardNumber();
  EXPECT_FALSE(IsCardMarked(page, card_number));
}

TEST_F(CardTableTest, MultipleObjects) {
  std::vector<BaseObject*> objects;
  BaseObject* obj = MakeGarbageCollected<Object<kCardSize>>();
  BasePage* const first_page = PageFromObject(obj);
  BasePage* new_page = first_page;

  while (first_page == new_page) {
    objects.push_back(obj);
    MarkCardForObject(obj);

    obj = MakeGarbageCollected<Object<kCardSize>>();
    new_page = PageFromObject(obj);
  }

  CheckObjects(objects, *static_cast<NormalPage*>(first_page));
}

}  // namespace blink
