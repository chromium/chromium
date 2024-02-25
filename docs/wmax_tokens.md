# -Wmax-tokens

**Update 2022-08-02** The -Wmax-tokens experiment was [retired](https://chromium-review.googlesource.com/c/chromium/src/+/3804719)
as the downsides (annoyance to developers) were determined to outweigh the
benefits (preventing include bloat), especially in the face of
[libc++ rolls](https://crbug.com/1348349).  We now have better
[include graph analysis](https://commondatastorage.googleapis.com/chromium-browser-clang/chrome_includes-index.html)
and [tracking of total build size over time](https://commondatastorage.googleapis.com/chromium-browser-clang/chrome_includes-index.html).
The goal is to have a [Gerrit warning for flagging include bloat
growth](https://crbug.com/1229609) in the future.

---

This is an experiment that uses the compiler to limit the size of certain header
files as a way of tackling #include bloat.

The main idea is to insert "back stops" for header sizes to ensure that header
cleanups don't regress without anyone noticing.

A back stop is implementing using this pragma:

```c++
#pragma clang max_tokens_here <num>
```

It makes the compiler issue a -Wmax-tokens warning (which we treat as an error)
if the number of tokens after preprocessing at that point in the translation
unit exceeds num.

For more background and data, see
[clang -Wmax-tokens: A Backstop for #include Bloat](https://docs.google.com/document/d/1xMkTZMKx9llnMPgso0jrx3ankI4cv60xeZ0y4ksf4wc/preview).


## What to do when hitting a -Wmax-tokens warning

The purpose of the pragma is to avoid _accidentally_ growing the size of widely
included headers. That is, they are there to make developers aware of an
increase, not to block it.

There are two common scenarios for hitting the pragma:

1. An incremental increase in header size. If the increase is small, don't worry
   about it (unless you want to), and just bump the limit.

1. If the increase is significant, say a doubling of the size or more, consider
   using techniques such as forward declarations to avoid increasing the header
   size. Even complex classes may have forward declarations available, see for
   example
   [https://source.chromium.org/chromium/chromium/src/+/HEAD:base/functional/callback_forward.h](callback_forward.h).
   Many types defined in .mojom.h files have forward declarations in a
   corresponding .mojom-forward.h file. If the size increase is unavoidable,
   raise the limit.

## How to insert a header size back stop

To limit the size of a header foo.h, insert the pragma directly after including
the header into its corresponding .cc file:

```c++
// in foo.cc:
#include "foo.h"
#pragma clang max_tokens_here 123456

#include "blah.h"
```

By inserting the pragma at this point, it captures the token count from
including the header in isolation.

In order to not create unnecessary friction for developers, token limits should
only be imposed on headers with significant impact on build times: headers that
are (or were) large and widely included.



## Experiences

- System headers on Chrome OS differ between boards and are not covered by the
  commit queue. This means the token limits were not tailored to those builds,
  causing build problems downstream. To avoid this, the -Wmax-tokens warning
  was disabled for Chrome OS (see
  [crbug.com/1079053](https://crbug.com/1079053)).

- Good examples of -Wmax-tokens preventing regressions:

  - [CL 2795166](
    https://chromium-review.googlesource.com/c/chromium/src/+/2795166): The CQ
    caught a size increase in render_frame_host.h, and the code was easily
    adjusted to avoid the increase.
