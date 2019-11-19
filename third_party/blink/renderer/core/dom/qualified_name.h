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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_QUALIFIED_NAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_QUALIFIED_NAME_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

struct QualifiedNameComponents {
  DISALLOW_NEW();
  StringImpl* prefix_;
  StringImpl* local_name_;
  StringImpl* namespace_;
};

// This struct is used to pass data between QualifiedName and the
// QNameTranslator.  For hashing and equality only the QualifiedNameComponents
// fields are used.
struct QualifiedNameData {
  DISALLOW_NEW();
  QualifiedNameComponents components_;
  bool is_static_;
};

CORE_EXPORT extern const class QualifiedName& g_null_name;

class CORE_EXPORT QualifiedName {
  USING_FAST_MALLOC(QualifiedName);

 public:
  class CORE_EXPORT QualifiedNameImpl : public RefCounted<QualifiedNameImpl> {
   public:
    static scoped_refptr<QualifiedNameImpl> Create(
        const AtomicString& prefix,
        const AtomicString& local_name,
        const AtomicString& namespace_uri,
        bool is_static) {
      return base::AdoptRef(
          new QualifiedNameImpl(prefix, local_name, namespace_uri, is_static));
    }

    ~QualifiedNameImpl();

    unsigned ComputeHash() const;

    void AddRef() {
      if (is_static_)
        return;
      RefCounted<QualifiedNameImpl>::AddRef();
    }

    void Release() {
      if (is_static_)
        return;
      RefCounted<QualifiedNameImpl>::Release();
    }

    // We rely on StringHasher's HashMemory clearing out the top 8 bits when
    // doing hashing and use one of the bits for the is_static_ value.
    mutable unsigned existing_hash_ : 24;
    unsigned is_static_ : 1;
    const AtomicString prefix_;
    const AtomicString local_name_;
    const AtomicString namespace_;
    mutable AtomicString local_name_upper_;

   private:
    QualifiedNameImpl(const AtomicString& prefix,
                      const AtomicString& local_name,
                      const AtomicString& namespace_uri,
                      bool is_static)
        : existing_hash_(0),
          is_static_(is_static),
          prefix_(prefix),
          local_name_(local_name),
          namespace_(namespace_uri)

    {
      DCHECK(!namespace_uri.IsEmpty() || namespace_uri.IsNull());
    }
  };

  QualifiedName(const AtomicString& prefix,
                const AtomicString& local_name,
                const AtomicString& namespace_uri);
  ~QualifiedName();

  QualifiedName(const QualifiedName& other) = default;
  const QualifiedName& operator=(const QualifiedName& other) {
    impl_ = other.impl_;
    return *this;
  }

  bool operator==(const QualifiedName& other) const {
    return impl_ == other.impl_;
  }
  bool operator!=(const QualifiedName& other) const {
    return !(*this == other);
  }

  bool Matches(const QualifiedName& other) const {
    return impl_ == other.impl_ || (LocalName() == other.LocalName() &&
                                    NamespaceURI() == other.NamespaceURI());
  }

  bool HasPrefix() const { return impl_->prefix_ != g_null_atom; }
  void SetPrefix(const AtomicString& prefix) {
    *this = QualifiedName(prefix, LocalName(), NamespaceURI());
  }

  const AtomicString& Prefix() const { return impl_->prefix_; }
  const AtomicString& LocalName() const { return impl_->local_name_; }
  const AtomicString& NamespaceURI() const { return impl_->namespace_; }

  // Uppercased localName, cached for efficiency
  const AtomicString& LocalNameUpper() const {
    if (impl_->local_name_upper_)
      return impl_->local_name_upper_;
    return LocalNameUpperSlow();
  }

  const AtomicString& LocalNameUpperSlow() const;

  String ToString() const;

  QualifiedNameImpl* Impl() const { return impl_.get(); }

  // Init routine for globals
  static void InitAndReserveCapacityForSize(unsigned size);

  static const QualifiedName& Null() { return g_null_name; }

  // The below methods are only for creating static global QNames that need no
  // ref counting.
  static void CreateStatic(void* target_address, StringImpl* name);
  static void CreateStatic(void* target_address,
                           StringImpl* name,
                           const AtomicString& name_namespace);

 private:
  friend struct WTF::HashTraits<blink::QualifiedName>;

  // This constructor is used only to create global/static QNames that don't
  // require any ref counting.
  QualifiedName(const AtomicString& prefix,
                const AtomicString& local_name,
                const AtomicString& namespace_uri,
                bool is_static);

  scoped_refptr<QualifiedNameImpl> impl_;
};

extern const QualifiedName& g_any_name;
inline const QualifiedName& AnyQName() {
  return g_any_name;
}

inline bool operator==(const AtomicString& a, const QualifiedName& q) {
  return a == q.LocalName();
}
inline bool operator!=(const AtomicString& a, const QualifiedName& q) {
  return a != q.LocalName();
}
inline bool operator==(const QualifiedName& q, const AtomicString& a) {
  return a == q.LocalName();
}
inline bool operator!=(const QualifiedName& q, const AtomicString& a) {
  return a != q.LocalName();
}

inline unsigned HashComponents(const QualifiedNameComponents& buf) {
  return StringHasher::HashMemory<sizeof(QualifiedNameComponents)>(&buf);
}

struct CORE_EXPORT QualifiedNameHash {
  STATIC_ONLY(QualifiedNameHash);
  static unsigned GetHash(const QualifiedName& name) {
    return GetHash(name.Impl());
  }

  static unsigned GetHash(const QualifiedName::QualifiedNameImpl* name) {
    if (!name->existing_hash_)
      name->existing_hash_ = name->ComputeHash();
    return name->existing_hash_;
  }

  static bool Equal(const QualifiedName& a, const QualifiedName& b) {
    return a == b;
  }
  static bool Equal(const QualifiedName::QualifiedNameImpl* a,
                    const QualifiedName::QualifiedNameImpl* b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const QualifiedName&);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::QualifiedName)

namespace WTF {

template <>
struct DefaultHash<blink::QualifiedName> {
  typedef blink::QualifiedNameHash Hash;
};

template <>
struct HashTraits<blink::QualifiedName>
    : SimpleClassHashTraits<blink::QualifiedName> {
  static const bool kEmptyValueIsZero = false;
  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const blink::QualifiedName& value) {
    return value == EmptyValue();
  }
  static const blink::QualifiedName& EmptyValue() {
    return blink::QualifiedName::Null();
  }

  static bool IsDeletedValue(const blink::QualifiedName& value) {
    using QualifiedNameImpl = blink::QualifiedName::QualifiedNameImpl;
    return HashTraits<scoped_refptr<QualifiedNameImpl>>::IsDeletedValue(
        value.impl_);
  }

  static void ConstructDeletedValue(blink::QualifiedName& slot,
                                    bool zero_value) {
    using QualifiedNameImpl = blink::QualifiedName::QualifiedNameImpl;
    HashTraits<scoped_refptr<QualifiedNameImpl>>::ConstructDeletedValue(
        slot.impl_, zero_value);
  }
};

}  // namespace WTF

#endif
