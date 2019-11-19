# WTF (Web Template Framework)

WTF is a base library for Blink providing a variety of basic functionalities,
like containers, string libraries, reference counting mechanisms, functors,
threading primitives etc.

WTF's mission is to power and support all other Blink code base by providing
fast, reliable, user-friendly and secure generic primitives.

Dependency-wise, WTF cannot depend on any other Blink headers, including
files under other platform/ subdirectories, since WTF is a library that can be
referred from anywhere in Blink. WTF basically can only depend on [base].

Main code base of Blink (core and modules) cannot directly depend on [base].
The main objective of this is to limit what Blink core can use, so that Blink
core code wouldn't use libraries prohibited in Blink accidentally (such as
`std::string` or `std::vector`).

Our approach here is to make WTF and other platform/ components a gatekeeper of
the use of [base] libraries. platform/ (including WTF) can directly depend on
[base], and if some of the [base] functionalities are worth having in Blink,
WTF can expose them in a way Blink core code can easily use them. Usually,
such a library would be a thin wrapper of the corresponding [base]
functionality.

Also, we are trying to eliminate duplicated functionalities between WTF and
[base]. Historically, WTF was developed as a stand-alone library, so there
are still many overlaps. We want to eventually delegate shareable implementation
to [base] as much as possible.

If you find a missing functionality in WTF, regardless of whether it is
available in [base] or not, feel free to file a bug under the component
Blink>Internals>WTF.

## Library catalog

The below is a list of major libraries. For a complete list, look at
[the directory listing].

* **Containers**

  [Vector], [HashSet], [HashMap], [Deque]

* **Strings**

  [String], [AtomicString], [StringBuilder]

* **Reference counting**

  [RefCounted]

* **Memory**

  [allocator.h] (memory placement macros)

* **Functors, binding**

  [functional.h]

* **Threading**

  [threading.h], [threading_primitives.h]

* **Miscellaneous**

  [std_lib_extras.h] (`DEFINE_STATIC_LOCAL` etc.),
  [time.h]

## History

The name WTF first [appeared in 2006][1], as a replacement of its older name
KXMLCore. At that point, there were already plenty of libraries we see today.
For example, you can see [the initial implementation of `Vector`][2] was landed
in 2006, replacing several manual array allocations and deallocations(!).

If you dig the repository a bit more, you can find the original version of
Assertions.h was [committed back in 2002][3]. This is probably the oldest
library that we can find from the repository history.

As you see, pretty much everything that we have today in WTF was created in
the WebKit era. WTF was initially under the directory Source/JavaScriptCore,
but it moved to Source/WTF/wtf in 2011-2012, then to Source/wtf in 2013.

Blink forked WebKit in 2013. In 2017, the directory finally [moved to the
current location][4] platform/wtf.

[the directory listing]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/
[base]: https://cs.chromium.org/chromium/src/base/
[Vector]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/vector.h
[HashSet]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/hash_set.h
[HashMap]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/hash_map.h
[Deque]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/deque.h
[String]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/text/wtf_string.h
[AtomicString]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/text/atomic_string.h
[StringBuilder]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/text/string_builder.h
[RefCounted]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/ref_counted.h
[allocator.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/allocator.h
[functional.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/functional.h
[threading.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/threading.h
[threading_primitives.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/threading_primitives.h
[build_config.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/build_config.h
[std_lib_extras.h]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/wtf/std_lib_extras.h
[1]: https://chromium.googlesource.com/chromium/src/+/e372c152fc6e57743ebc508fe17f6eb131b4ff8d
[2]: https://chromium.googlesource.com/chromium/src/+/547a6ca360a56fbee3d5ea4a71ba18f91622455c
[3]: https://chromium.googlesource.com/chromium/src/+/478890427ee03fd88e6f0f58ee8220512044bed9/third_party/WebKit/WebCore/kwq/KWQAssertions.h
[4]:https://docs.google.com/document/d/1JK26H-1-cD9-s9QLvEfY55H2kgSxRFNPLfjs049Us5w/edit?usp=sharing
