/*
 * Copyright (C) 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/qualified_name.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

struct SameSizeAsQualifiedNameImpl
    : public RefCounted<SameSizeAsQualifiedNameImpl> {
  unsigned bitfield;
  void* pointers[4];
};

ASSERT_SIZE(QualifiedName::QualifiedNameImpl, SameSizeAsQualifiedNameImpl);

using QualifiedNameCache = HashSet<QualifiedName::QualifiedNameImpl*>;

static QualifiedNameCache& GetQualifiedNameCache() {
  // This code is lockless and thus assumes it all runs on one thread!
  DCHECK(IsMainThread());
  static QualifiedNameCache* g_name_cache = new QualifiedNameCache;
  return *g_name_cache;
}

struct QNameComponentsTranslator {
  static unsigned GetHash(const QualifiedNameData& data) {
    return HashComponents(data.components_);
  }
  static bool Equal(QualifiedName::QualifiedNameImpl* name,
                    const QualifiedNameData& data) {
    return data.components_.prefix_ == name->prefix_.Impl() &&
           data.components_.local_name_ == name->local_name_.Impl() &&
           data.components_.namespace_ == name->namespace_.Impl();
  }
  static void Store(QualifiedName::QualifiedNameImpl*& location,
                    const QualifiedNameData& data,
                    unsigned) {
    const QualifiedNameComponents& components = data.components_;
    auto name = QualifiedName::QualifiedNameImpl::Create(
        components.prefix_, components.local_name_, components.namespace_,
        data.is_static_);
    name->AddRef();
    location = name.get();
  }
};

QualifiedName::QualifiedName(const AtomicString& p,
                             const AtomicString& l,
                             const AtomicString& n) {
  QualifiedNameData data = {
      {p.Impl(), l.Impl(), n.empty() ? g_null_atom.Impl() : n.Impl()}, false};
  QualifiedNameCache::AddResult add_result =
      GetQualifiedNameCache().AddWithTranslator<QNameComponentsTranslator>(
          data);
  impl_ = *add_result.stored_value;
  if (add_result.is_new_entry)
    impl_->Release();
}

QualifiedName::QualifiedName(const AtomicString& local_name)
    : QualifiedName(g_null_atom, local_name, g_null_atom) {}

QualifiedName::QualifiedName(const AtomicString& p,
                             const AtomicString& l,
                             const AtomicString& n,
                             bool is_static) {
  QualifiedNameData data = {{p.Impl(), l.Impl(), n.Impl()}, is_static};
  QualifiedNameCache::AddResult add_result =
      GetQualifiedNameCache().AddWithTranslator<QNameComponentsTranslator>(
          data);
  impl_ = *add_result.stored_value;
  if (add_result.is_new_entry)
    impl_->Release();
}

QualifiedName::~QualifiedName() = default;

QualifiedName::QualifiedNameImpl::~QualifiedNameImpl() {
  GetQualifiedNameCache().erase(this);
}

String QualifiedName::ToString() const {
  String local = LocalName();
  if (HasPrefix())
    return Prefix().GetString() + ":" + local;
  return local;
}

// Global init routines
DEFINE_GLOBAL(QualifiedName, g_any_name);
DEFINE_GLOBAL(QualifiedName, g_null_name);

void QualifiedName::InitAndReserveCapacityForSize(unsigned size) {
  DCHECK(g_star_atom.Impl());
  GetQualifiedNameCache().ReserveCapacityForSize(
      size + 2 /*g_star_atom and g_null_atom */);
  new ((void*)&g_any_name)
      QualifiedName(g_null_atom, g_null_atom, g_star_atom, true);
  new ((void*)&g_null_name)
      QualifiedName(g_null_atom, g_null_atom, g_null_atom, true);
}

const AtomicString& QualifiedName::LocalNameUpperSlow() const {
  impl_->local_name_upper_ = impl_->local_name_.UpperASCII();
  return impl_->local_name_upper_;
}

unsigned QualifiedName::QualifiedNameImpl::ComputeHash() const {
  QualifiedNameComponents components = {prefix_.Impl(), local_name_.Impl(),
                                        namespace_.Impl()};
  return HashComponents(components);
}

void QualifiedName::CreateStatic(void* target_address,
                                 StringImpl* name,
                                 const AtomicString& name_namespace) {
  new (target_address)
      QualifiedName(g_null_atom, AtomicString(name), name_namespace, true);
}

void QualifiedName::CreateStatic(void* target_address, StringImpl* name) {
  new (target_address)
      QualifiedName(g_null_atom, AtomicString(name), g_null_atom, true);
}

std::ostream& operator<<(std::ostream& ostream, const QualifiedName& qname) {
  ostream << "QualifiedName(local=" << qname.LocalName()
          << " ns=" << qname.NamespaceURI() << " prefix=" << qname.Prefix()
          << ")";
  return ostream;
}

}  // namespace blink
