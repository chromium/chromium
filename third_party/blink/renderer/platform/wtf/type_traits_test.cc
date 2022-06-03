/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/type_traits.h"

#include "build/build_config.h"

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

// No gtest tests; only static_assert checks.

namespace WTF {

namespace {

struct VirtualClass {
  virtual void A() {}
};
static_assert(!std::is_trivially_move_assignable<VirtualClass>::value,
              "VirtualClass should not be trivially move assignable");

struct DestructorClass {
  ~DestructorClass() = default;
};
static_assert(std::is_trivially_move_assignable<DestructorClass>::value,
              "DestructorClass should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<DestructorClass>::value,
              "DestructorClass should be trivially copy assignable");
static_assert(std::is_default_constructible<DestructorClass>::value,
              "DestructorClass should be default constructible");

struct MixedPrivate {
  int M2() { return m2; }
  int m1;

 private:
  int m2;
};
static_assert(std::is_trivially_move_assignable<MixedPrivate>::value,
              "MixedPrivate should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<MixedPrivate>::value,
              "MixedPrivate should be trivially copy assignable");
static_assert(std::is_trivially_default_constructible<MixedPrivate>::value,
              "MixedPrivate should have a trivial default constructor");
struct JustPrivate {
  int M2() { return m2; }

 private:
  int m2;
};
static_assert(std::is_trivially_move_assignable<JustPrivate>::value,
              "JustPrivate should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<JustPrivate>::value,
              "JustPrivate should be trivially copy assignable");
static_assert(std::is_trivially_default_constructible<JustPrivate>::value,
              "JustPrivate should have a trivial default constructor");
struct JustPublic {
  int m2;
};
static_assert(std::is_trivially_move_assignable<JustPublic>::value,
              "JustPublic should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<JustPublic>::value,
              "JustPublic should be trivially copy assignable");
static_assert(std::is_trivially_default_constructible<JustPublic>::value,
              "JustPublic should have a trivial default constructor");
struct NestedInherited : public JustPublic, JustPrivate {
  float m3;
};
static_assert(std::is_trivially_move_assignable<NestedInherited>::value,
              "NestedInherited should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<NestedInherited>::value,
              "NestedInherited should be trivially copy assignable");
static_assert(std::is_trivially_default_constructible<NestedInherited>::value,
              "NestedInherited should have a trivial default constructor");
struct NestedOwned {
  JustPublic m1;
  JustPrivate m2;
  float m3;
};

static_assert(std::is_trivially_move_assignable<NestedOwned>::value,
              "NestedOwned should be trivially move assignable");
static_assert(std::is_trivially_copy_assignable<NestedOwned>::value,
              "NestedOwned should be trivially copy assignable");
static_assert(std::is_trivially_default_constructible<NestedOwned>::value,
              "NestedOwned should have a trivial default constructor");

class NonCopyableClass {
 public:
  NonCopyableClass(const NonCopyableClass&) = delete;
  NonCopyableClass& operator=(const NonCopyableClass&) = delete;
};

static_assert(!std::is_trivially_move_assignable<NonCopyableClass>::value,
              "NonCopyableClass should not be trivially move assignable");
static_assert(!std::is_trivially_copy_assignable<NonCopyableClass>::value,
              "NonCopyableClass should not be trivially copy assignable");
static_assert(!std::is_trivially_default_constructible<NonCopyableClass>::value,
              "NonCopyableClass should not have a trivial default constructor");

template <typename T>
class TestBaseClass {};

class TestDerivedClass : public TestBaseClass<int> {};

static_assert((IsSubclass<TestDerivedClass, TestBaseClass<int>>::value),
              "Derived class should be a subclass of its base");
static_assert((!IsSubclass<TestBaseClass<int>, TestDerivedClass>::value),
              "Base class should not be a sublass of a derived class");
static_assert((IsSubclassOfTemplate<TestDerivedClass, TestBaseClass>::value),
              "Derived class should be a subclass of template from its base");

typedef int IntArray[];
typedef int IntArraySized[4];

#if !defined(COMPILER_MSVC) || defined(__clang__)

class AssignmentDeleted final {
  STACK_ALLOCATED();

 private:
  AssignmentDeleted& operator=(const AssignmentDeleted&) = delete;
};

static_assert(!std::is_copy_assignable<AssignmentDeleted>::value,
              "AssignmentDeleted isn't copy assignable.");
static_assert(!std::is_move_assignable<AssignmentDeleted>::value,
              "AssignmentDeleted isn't move assignable.");

class AssignmentPrivate final {
  STACK_ALLOCATED();

 private:
  AssignmentPrivate& operator=(const AssignmentPrivate&);
};

static_assert(!std::is_copy_assignable<AssignmentPrivate>::value,
              "AssignmentPrivate isn't copy assignable.");
static_assert(!std::is_move_assignable<AssignmentPrivate>::value,
              "AssignmentPrivate isn't move assignable.");

class CopyAssignmentDeleted final {
  STACK_ALLOCATED();

 public:
  CopyAssignmentDeleted& operator=(CopyAssignmentDeleted&&);

 private:
  CopyAssignmentDeleted& operator=(const CopyAssignmentDeleted&) = delete;
};

static_assert(!std::is_copy_assignable<CopyAssignmentDeleted>::value,
              "CopyAssignmentDeleted isn't copy assignable.");
static_assert(std::is_move_assignable<CopyAssignmentDeleted>::value,
              "CopyAssignmentDeleted is move assignable.");

class CopyAssignmentPrivate final {
  STACK_ALLOCATED();

 public:
  CopyAssignmentPrivate& operator=(CopyAssignmentPrivate&&);

 private:
  CopyAssignmentPrivate& operator=(const CopyAssignmentPrivate&);
};

static_assert(!std::is_copy_assignable<CopyAssignmentPrivate>::value,
              "CopyAssignmentPrivate isn't copy assignable.");
static_assert(std::is_move_assignable<CopyAssignmentPrivate>::value,
              "CopyAssignmentPrivate is move assignable.");

class CopyAssignmentUndeclared final {
  STACK_ALLOCATED();

 public:
  CopyAssignmentUndeclared& operator=(CopyAssignmentUndeclared&&);
};

static_assert(!std::is_copy_assignable<CopyAssignmentUndeclared>::value,
              "CopyAssignmentUndeclared isn't copy assignable.");
static_assert(std::is_move_assignable<CopyAssignmentUndeclared>::value,
              "CopyAssignmentUndeclared is move assignable.");

class Assignable final {
  STACK_ALLOCATED();

 public:
  Assignable& operator=(const Assignable&);
};

static_assert(std::is_copy_assignable<Assignable>::value,
              "Assignable is copy assignable.");
static_assert(std::is_move_assignable<Assignable>::value,
              "Assignable is move assignable.");

class AssignableImplicit final {};

static_assert(std::is_copy_assignable<AssignableImplicit>::value,
              "AssignableImplicit is copy assignable.");
static_assert(std::is_move_assignable<AssignableImplicit>::value,
              "AssignableImplicit is move assignable.");

#endif  // !defined(COMPILER_MSVC) || defined(__clang__)

class DefaultConstructorDeleted final {
  STACK_ALLOCATED();

 private:
  DefaultConstructorDeleted() = delete;
};

class DestructorDeleted final {
  STACK_ALLOCATED();

 private:
  ~DestructorDeleted() = delete;
};

static_assert(
    !std::is_trivially_default_constructible<DefaultConstructorDeleted>::value,
    "DefaultConstructorDeleted must not be trivially default constructible.");

static_assert(!std::is_trivially_destructible<DestructorDeleted>::value,
              "DestructorDeleted must not be trivially destructible.");

#define EnsurePtrConvertibleArgDecl(From, To)                              \
  typename std::enable_if<std::is_convertible<From*, To*>::value>::type* = \
      nullptr

template <typename T>
class Wrapper {
 public:
  template <typename U>
  Wrapper(const Wrapper<U>&, EnsurePtrConvertibleArgDecl(U, T)) {}
};

class ForwardDeclarationOnlyClass;

static_assert(std::is_convertible<Wrapper<TestDerivedClass>,
                                  Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, T> should pass");

static_assert(std::is_convertible<Wrapper<TestDerivedClass>,
                                  Wrapper<const TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, const T> should pass");

static_assert(!std::is_convertible<Wrapper<const TestDerivedClass>,
                                   Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<const T, T> should not pass");

static_assert(std::is_convertible<Wrapper<ForwardDeclarationOnlyClass>,
                                  Wrapper<ForwardDeclarationOnlyClass>>::value,
              "EnsurePtrConvertibleArgDecl<T, T> should pass if T is not a "
              "complete type");

static_assert(
    std::is_convertible<Wrapper<ForwardDeclarationOnlyClass>,
                        Wrapper<const ForwardDeclarationOnlyClass>>::value,
    "EnsurePtrConvertibleArgDecl<T, const T> should pass if T is not a "
    "complete type");

static_assert(!std::is_convertible<Wrapper<const ForwardDeclarationOnlyClass>,
                                   Wrapper<ForwardDeclarationOnlyClass>>::value,
              "EnsurePtrConvertibleArgDecl<const T, T> should not pass if T is "
              "not a complete type");

static_assert(
    std::is_convertible<Wrapper<TestDerivedClass>,
                        Wrapper<TestBaseClass<int>>>::value,
    "EnsurePtrConvertibleArgDecl<U, T> should pass if U is a subclass of T");

static_assert(!std::is_convertible<Wrapper<TestBaseClass<int>>,
                                   Wrapper<TestDerivedClass>>::value,
              "EnsurePtrConvertibleArgDecl<U, T> should not pass if U is a "
              "base class of T");

}  // anonymous namespace

}  // namespace WTF
