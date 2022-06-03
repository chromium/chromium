/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/mutation_record.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

class ChildListRecord : public MutationRecord {
 public:
  ChildListRecord(Node* target,
                  StaticNodeList* added,
                  StaticNodeList* removed,
                  Node* previous_sibling,
                  Node* next_sibling)
      : target_(target),
        added_nodes_(added),
        removed_nodes_(removed),
        previous_sibling_(previous_sibling),
        next_sibling_(next_sibling) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    visitor->Trace(added_nodes_);
    visitor->Trace(removed_nodes_);
    visitor->Trace(previous_sibling_);
    visitor->Trace(next_sibling_);
    MutationRecord::Trace(visitor);
  }

 private:
  const AtomicString& type() override;
  Node* target() override { return target_.Get(); }
  StaticNodeList* addedNodes() override { return added_nodes_.Get(); }
  StaticNodeList* removedNodes() override { return removed_nodes_.Get(); }
  Node* previousSibling() override { return previous_sibling_.Get(); }
  Node* nextSibling() override { return next_sibling_.Get(); }

  Member<Node> target_;
  Member<StaticNodeList> added_nodes_;
  Member<StaticNodeList> removed_nodes_;
  Member<Node> previous_sibling_;
  Member<Node> next_sibling_;
};

class RecordWithEmptyNodeLists : public MutationRecord {
 public:
  RecordWithEmptyNodeLists(Node* target, const String& old_value)
      : target_(target), old_value_(old_value) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    visitor->Trace(added_nodes_);
    visitor->Trace(removed_nodes_);
    MutationRecord::Trace(visitor);
  }

 private:
  Node* target() override { return target_.Get(); }
  String oldValue() override { return old_value_; }
  StaticNodeList* addedNodes() override {
    return LazilyInitializeEmptyNodeList(added_nodes_);
  }
  StaticNodeList* removedNodes() override {
    return LazilyInitializeEmptyNodeList(removed_nodes_);
  }

  static StaticNodeList* LazilyInitializeEmptyNodeList(
      Member<StaticNodeList>& node_list) {
    if (!node_list)
      node_list = MakeGarbageCollected<StaticNodeList>();
    return node_list.Get();
  }

  Member<Node> target_;
  String old_value_;
  Member<StaticNodeList> added_nodes_;
  Member<StaticNodeList> removed_nodes_;
};

class AttributesRecord : public RecordWithEmptyNodeLists {
 public:
  AttributesRecord(Node* target,
                   const QualifiedName& name,
                   const AtomicString& old_value)
      : RecordWithEmptyNodeLists(target, old_value),
        attribute_name_(name.LocalName()),
        attribute_namespace_(name.NamespaceURI()) {}

 private:
  const AtomicString& type() override;
  const AtomicString& attributeName() override { return attribute_name_; }
  const AtomicString& attributeNamespace() override {
    return attribute_namespace_;
  }

  AtomicString attribute_name_;
  AtomicString attribute_namespace_;
};

class CharacterDataRecord : public RecordWithEmptyNodeLists {
 public:
  CharacterDataRecord(Node* target, const String& old_value)
      : RecordWithEmptyNodeLists(target, old_value) {}

 private:
  const AtomicString& type() override;
};

class MutationRecordWithNullOldValue : public MutationRecord {
 public:
  MutationRecordWithNullOldValue(MutationRecord* record) : record_(record) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(record_);
    MutationRecord::Trace(visitor);
  }

 private:
  const AtomicString& type() override { return record_->type(); }
  Node* target() override { return record_->target(); }
  StaticNodeList* addedNodes() override { return record_->addedNodes(); }
  StaticNodeList* removedNodes() override { return record_->removedNodes(); }
  Node* previousSibling() override { return record_->previousSibling(); }
  Node* nextSibling() override { return record_->nextSibling(); }
  const AtomicString& attributeName() override {
    return record_->attributeName();
  }
  const AtomicString& attributeNamespace() override {
    return record_->attributeNamespace();
  }

  String oldValue() override { return String(); }

  Member<MutationRecord> record_;
};

const AtomicString& ChildListRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, child_list, ("childList"));
  return child_list;
}

const AtomicString& AttributesRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, attributes, ("attributes"));
  return attributes;
}

const AtomicString& CharacterDataRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, character_data, ("characterData"));
  return character_data;
}

}  // namespace

MutationRecord* MutationRecord::CreateChildList(Node* target,
                                                StaticNodeList* added,
                                                StaticNodeList* removed,
                                                Node* previous_sibling,
                                                Node* next_sibling) {
  return MakeGarbageCollected<ChildListRecord>(target, added, removed,
                                               previous_sibling, next_sibling);
}

MutationRecord* MutationRecord::CreateAttributes(
    Node* target,
    const QualifiedName& name,
    const AtomicString& old_value) {
  return MakeGarbageCollected<AttributesRecord>(target, name, old_value);
}

MutationRecord* MutationRecord::CreateCharacterData(Node* target,
                                                    const String& old_value) {
  return MakeGarbageCollected<CharacterDataRecord>(target, old_value);
}

MutationRecord* MutationRecord::CreateWithNullOldValue(MutationRecord* record) {
  return MakeGarbageCollected<MutationRecordWithNullOldValue>(record);
}

MutationRecord::~MutationRecord() = default;

}  // namespace blink
