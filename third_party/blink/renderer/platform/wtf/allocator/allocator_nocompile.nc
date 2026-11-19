// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "partition_alloc/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "v8/include/cppgc/garbage-collected.h"

namespace blink {

class DirectGC : public cppgc::GarbageCollected<DirectGC> {
  USING_FAST_MALLOC(DirectGC);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class DirectGCWithTypeName
    : public cppgc::GarbageCollected<DirectGCWithTypeName> {
  USING_FAST_MALLOC_WITH_TYPE_NAME(DirectGCWithTypeName);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class PrivateGC : private cppgc::GarbageCollected<PrivateGC> {
  USING_FAST_MALLOC(PrivateGC);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class BaseGC : public cppgc::GarbageCollected<BaseGC> {};

class IndirectGC : public BaseGC {
  USING_FAST_MALLOC(IndirectGC);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class DirectMixin : public cppgc::GarbageCollectedMixin {
  USING_FAST_MALLOC(DirectMixin);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class MixinBaseA : public cppgc::GarbageCollectedMixin {};
class MixinBaseB : public cppgc::GarbageCollectedMixin {};

class DiamondMixin : public MixinBaseA, public MixinBaseB {
  USING_FAST_MALLOC(DiamondMixin);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

template <typename T>
class TemplateGC : public cppgc::GarbageCollected<TemplateGC<T>> {
  USING_FAST_MALLOC(TemplateGC);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};
TemplateGC<int> g_template_gc;

// RefCounted<T> invokes USING_FAST_MALLOC(T) while T is incomplete; ensure that
// does not memoize a false result for the subsequent check inside the derived
// class body.
class RefCountedAndGC : public RefCounted<RefCountedAndGC>,
                        public cppgc::GarbageCollected<RefCountedAndGC> {
  USING_FAST_MALLOC(RefCountedAndGC);  // expected-error {{USING_FAST_MALLOC cannot be used on a GarbageCollected type}}
};

class BaseDisallowNew {
  DISALLOW_NEW();
};

class DerivedFromDisallowNew : public BaseDisallowNew {
  USING_FAST_MALLOC(DerivedFromDisallowNew);  // expected-error {{USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type}}
};

class PrivateDerivedFromDisallowNew : private BaseDisallowNew {
  USING_FAST_MALLOC(PrivateDerivedFromDisallowNew);  // expected-error {{USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type}}
};

class OtherBaseDisallowNew {
  DISALLOW_NEW();
};

class DiamondDisallowNew : public BaseDisallowNew, public OtherBaseDisallowNew {
  USING_FAST_MALLOC(DiamondDisallowNew);  // expected-error {{USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type}}
};

class BaseStackAllocated {
  STACK_ALLOCATED();
};

class DerivedFromStackAllocated : public BaseStackAllocated {
  USING_FAST_MALLOC(DerivedFromStackAllocated);  // expected-error {{USING_FAST_MALLOC cannot be used on a STACK_ALLOCATED type}}
};

class SameClassDisallowNewBefore {
  DISALLOW_NEW();
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  USING_FAST_MALLOC(SameClassDisallowNewBefore);  // expected-error {{type alias redefinition with different types}} expected-error {{USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type}}
#else
  USING_FAST_MALLOC(SameClassDisallowNewBefore);  // expected-error 3 {{class member cannot be redeclared}} expected-error {{type alias redefinition with different types}} expected-error {{USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type}}
#endif
};

class SameClassDisallowNewAfter {
  USING_FAST_MALLOC(SameClassDisallowNewAfter);
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  DISALLOW_NEW();  // expected-error {{type alias redefinition with different types}}
#else
  DISALLOW_NEW();  // expected-error 3 {{class member cannot be redeclared}} expected-error {{type alias redefinition with different types}}
#endif
};

class SameClassStackAllocatedBefore {
  STACK_ALLOCATED();
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  USING_FAST_MALLOC(SameClassStackAllocatedBefore);  // expected-error {{USING_FAST_MALLOC cannot be used on a STACK_ALLOCATED type}}
#else
  USING_FAST_MALLOC(SameClassStackAllocatedBefore);  // expected-error 3 {{class member cannot be redeclared}} expected-error {{USING_FAST_MALLOC cannot be used on a STACK_ALLOCATED type}}
#endif
};

class DuplicateFastMalloc {
  USING_FAST_MALLOC(DuplicateFastMalloc);
  // Under MSVC compatibility mode (clang-cl / -fms-compatibility), identical
  // class-scope type alias redefinitions are permitted, so
  // `BlinkAllocatorMacroTag` only emits a redefinition error when `_MSC_VER`
  // is not defined. `FastMallocCheckTag` (and `operator new`/`delete` when
  // PA-E is disabled) still trigger redeclaration errors on all platforms.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(_MSC_VER)
  USING_FAST_MALLOC(DuplicateFastMalloc);  // expected-error {{class member cannot be redeclared}}
#elif PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  USING_FAST_MALLOC(DuplicateFastMalloc);  // expected-error {{redefinition of 'BlinkAllocatorMacroTag'}} expected-error {{class member cannot be redeclared}}
#elif defined(_MSC_VER)
  USING_FAST_MALLOC(DuplicateFastMalloc);  // expected-error 8 {{class member cannot be redeclared}}
#else
  USING_FAST_MALLOC(DuplicateFastMalloc);  // expected-error 8 {{class member cannot be redeclared}} expected-error {{redefinition of 'BlinkAllocatorMacroTag'}}
#endif
};

}  // namespace blink
