# Android Java <-> C++ Ownership Best Practices

Aims to provide best practices for maintaining the ownership and lifecycle of
logically paired Java / C++ objects (e.g. if you have a Foo object in Java and
one in C++ that are meant to represent the same concept).

[TOC]

## Establish clear ownership
Either the Java or C++ object should own the lifecycle of the corresponding
object in the other language. The initially created object (either in C++ or
Java) should be the object that creates and owns the corresponding other
object.

It is the responsibility of the initial object to handle destruction / teardown
of the corresponding other object.

### [Option #1] Java owns the C++ counterpart
Because Java objects are garbage collected and finalizers are prohibited in
Chromium ([link](/styleguide/java/java.md#finalizers)), an explicit
destroy / teardown method on the Java object is required to prevent leaking the
corresponding C++ object. The destroy / teardown method on the Java object
would call an appropriate function on the C++ object (via JNI) to trigger the
deletion of the C++ object. At this point, the Java object should reset its
pointer reference to the C++ object to prevent any calls to the now destroyed
C++ instance.

### [Option #2] C++ owns the Java counterpart
For C++ objects, utilizing the appropriate smart java references
([link](/third_party/jni_zero/README.md#java-objects-and-garbage-collection),
[code ref](/base/android/scoped_java_ref.h)) will ensure corresponding Java
objects can be garbage collected. But if the Java object requires cleaning up
dependencies, the C++ object should call a corresponding teardown method on the
Java object in its destructor.

Even in cases where the Java object does not have dependencies requiring clean
up, the C++ object should notify the Java object that is has gone away. Then the
Java object can reset its pointer reference to the C++ object and prevent any
calls to the already destroyed object.

## Enforce relationship cardinality
There should be one Java object per native object (and vice versa) to keep the
lifecycle simple and easily understood.

For example, there is one BookmarkModel per Chrome profile in C++, and
therefore, there should only be one BookmarkModel instance per Profile in Java.

## Pick a side for your business logic
Where possible, keep the business logic in either C++ or Java, and have the
other object simply act as a shim to the other.

To facilitate cross-platform development, C++ is the preferred place for
business logic that could be shared in the future.

## Prefer colocation
The code of the Java and C++ object should be colocated to ensure consistent
layering and dependencies..

If the C++ object is in //components/[foo], then the corresponding Java object
should also reside in //components/[foo].

## Keep your C++ code close and your Java code closer
The C++ code shared across platforms and the corresponding Java class should be
as close as possible in the code.

For cases where there are just a few Java <-> C++ calls, try to simply inline
those into the same C++ file to minimize indirection.

**Example:**

//components/[foo]/foo_factory.cc
```c++
<...> cross platform includes

#if BUILDFLAG(IS_ANDROID)
#include “base/android/scoped_java_ref.h”
#include “components/[foo]/android/jni_headers/FooFactory_jni.h”
#endif  // BUILDFLAG(IS_ANDROID)

<...> shared functions

#if BUILDFLAG(IS_ANDROID)
static ScopedJavaLocalRef<jobject> JNI_FooFactory_Get(JNIEnv* env) {
    return FooFactory::Get()->GetJavaObject();
}
#endif  // BUILDFLAG(IS_ANDROID)
```

For cases where the Java <-> C++ API surface is substantial (e.g. if you have a
C++ object with a large public API and you want to expose all those functions to
Java), you can split out a JNI methods to a separate class that is owned by the
primary C++ object. This approach is suitable when we want to minimize the JNI
boilerplate in the C++ class.

**Example:**

//components/[foo]/foo.h
```c++
class Foo {
 public:
  <...>

#if BUILDFLAG(IS_ANDROID)
  void DoSomething();
#endif  // BUILDFLAG(IS_ANDROID)

 private:
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<FooAndroid> foo_android_;
#endif  // BUILDFLAG(IS_ANDROID)
}
```

//components/[foo]/foo.cc
```c++
<...>

#if BUILDFLAG(IS_ANDROID)
void Foo::DoSomething() {
  if (!foo_android_) {
    foo_android_ = std::make_unique<FooAndroid>(this);
  }
  foo_android_->DoSomething();
}
#endif  // BUILDFLAG(IS_ANDROID)
```

//components/[foo]/android/foo_android.h
```c++
class FooAndroid {
 public:
  void DoAThing();

  // JNI methods called from Java.
  void SomethingElse(JNIEnv* env);
  jboolean AndABooleanToo(JNIEnv* env);
  <...>

 private:
  const raw_ptr<Foo> foo_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
}
```

//components/[foo]/android/foo_android.cc
```c++
FooAndroid::FooAndroid(Foo* foo) : foo_(foo) {}

FooAndroid::DoAThing() {
  Java_Foo_DoAThing(base::android::AttachCurrentThread(), java_ref_);
}

void FooAndroid::SomethingElse(JNIEnv* env) {
  foo_->SomethingElse();
}

jboolean FooAndroid::AndABooleanToo(JNIEnv* env) {
  return foo->AndABooleanToo();
}
```

## When Lifetime is Hard
We do not allow the [use of finalizers](/styleguide/java/java.md#Finalizers),
but there are a couple of other tricks that have been used to clean up objects
besides explicit lifetimes:
1. Destroy and re-create the native object every time you need it
   ([GURL does this](/url/android/java/src/org/chromium/url/Parsed.java)).
2. Use a reference queue that is flushed every once in a while
   ([example](https://source.chromium.org/search?q=symbol:TaskRunnerImpl.destroyGarbageCollectedTaskRunners)).
