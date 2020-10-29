// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/marking_verifier.h"

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"

namespace blink {

void MarkingVerifier::VerifyObject(HeapObjectHeader* header) {
  // Verify only non-free marked objects.
  if (header->IsFree() || !header->IsMarked())
    return;

  const GCInfo& info = GCInfo::From(header->GcInfoIndex());
  const bool can_verify =
      !info.has_v_table || blink::VTableInitialized(header->Payload());
  if (can_verify) {
    parent_ = header;
    info.trace(this, header->Payload());
  }
}

void MarkingVerifier::Visit(const void* object, TraceDescriptor desc) {
  VerifyChild(object, desc.base_object_payload);
}

void MarkingVerifier::VisitWeak(const void* object,
                                const void* object_weak_ref,
                                TraceDescriptor desc,
                                WeakCallback callback) {
  // Weak objects should have been cleared at this point. As a consequence, all
  // objects found through weak references have to point to live objects at this
  // point.
  VerifyChild(object, desc.base_object_payload);
}

void MarkingVerifier::VisitWeakContainer(const void* object,
                                         const void* const*,
                                         TraceDescriptor,
                                         TraceDescriptor weak_desc,
                                         WeakCallback,
                                         const void*) {
  if (!object)
    return;

  // Contents of weak backing stores are found themselves through page
  // iteration and are treated strongly that way, similar to how they are
  // treated strongly when found through stack scanning. The verification
  // here only makes sure that the backing itself is properly marked. Weak
  // backing stores found through
  VerifyChild(object, weak_desc.base_object_payload);
}

void MarkingVerifier::VerifyChild(const void* object,
                                  const void* base_object_payload) {
  CHECK(object);
  // Verification may check objects that are currently under construction and
  // would require vtable access to figure out their headers. A nullptr in
  // |base_object_payload| indicates that a mixin object is in construction
  // and the vtable cannot be used to get to the object header.
  const HeapObjectHeader* const child_header =
      (base_object_payload) ? HeapObjectHeader::FromPayload(base_object_payload)
                            : HeapObjectHeader::FromInnerAddress(object);
  // These checks ensure that any children reachable from marked parents are
  // also marked. If you hit these checks then marking is in an inconsistent
  // state meaning that there are unmarked objects reachable from marked
  // ones.
  CHECK(child_header);
  if (!child_header->IsMarked()) {
    CHECK(!PageFromObject(child_header->Payload())->HasBeenSwept());
    LOG(FATAL) << "MarkingVerifier: Encountered unmarked object. " << std::endl
               << std::endl
               << "Hint: " << std::endl
               << parent_->Name() << std::endl
               << "\\-> " << child_header->Name() << std::endl;
  }
}

}  // namespace blink
