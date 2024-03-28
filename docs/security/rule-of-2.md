# The Rule Of 2

When you write code to parse, evaluate, or otherwise handle untrustworthy inputs
from the Internet — which is almost everything we do in a web browser! — we like
to follow a simple rule to make sure it's safe enough to do so. The Rule Of 2
is: Pick no more than 2 of

  * untrustworthy inputs;
  * unsafe implementation language; and
  * high privilege.

![alt text](rule-of-2-drawing.png "Venn diagram showing you should always use
a safe language, a sandbox, or not be processing untrustworthy inputs in the first
place.")

(drawing source
[here](https://docs.google.com/drawings/d/12WoPI7-E5NAINHUZqEPGn38aZBYBxq20BgVBjZIvgCQ/edit?usp=sharing))

## Why?

When code that handles untrustworthy inputs at high privilege has bugs, the
resulting vulnerabilities are typically of Critical or High severity. (See our
[Severity Guidelines](severity-guidelines.md).) We'd love to reduce the severity
of such bugs by reducing the amount of damage they can do (lowering their
privilege), avoiding the various types of memory corruption bugs (using a safe
language), or reducing the likelihood that the input is malicious (asserting the
trustworthiness of the source).

For the purposes of this document, our main concern is reducing (and hopefully,
ultimately eliminating) bugs that arise due to _memory unsafety_. [A recent
study by Matt Miller from Microsoft
Security](https://github.com/Microsoft/MSRC-Security-Research/blob/master/presentations/2019_02_BlueHatIL/2019_01%20-%20BlueHatIL%20-%20Trends%2C%20challenge%2C%20and%20shifts%20in%20software%20vulnerability%20mitigation.pdf)
states that "~70% of the vulnerabilities addressed through a security update
each year continue to be memory safety issues". A trip through Chromium's bug
tracker will show many, many vulnerabilities whose root cause is memory
unsafety. (As of March 2019, only about 5 of 130 [public Critical-severity
bugs](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=Type%3DBug-Security+Security_Severity%3DCritical+-status%3AWontFix+-status%3ADuplicate&sort=&groupby=&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&mode=&cells=ids&num=)
are not obviously due to memory corruption.)

Security engineers in general, very much including Chrome Security Team, would
like to advance the state of engineering to where memory safety issues are much
more rare. Then, we could focus more attention on the application-semantic
vulnerabilities. 😊 That would be a big improvement.

## What?

Some definitions are in order.

### Untrustworthy Inputs

_Untrustworthy inputs_ are inputs that

  * have non-trivial grammars; and/or
  * come from untrustworthy sources.

If there were an input type so simple that it were straightforward to write a
memory-safe handler for it, we wouldn't need to worry much about where it came
from **for the purposes of memory safety**, because we'd be sure we could handle
it. We would still need to treat the input as untrustworthy after
parsing, of course.

Unfortunately, it is very rare to find a grammar trivial enough that we can
trust ourselves to parse it successfully or fail safely. (But see
[Normalization](#normalization) for a potential example.) Therefore, we do need
to concern ourselves with the provenance of such inputs.

Any arbitrary peer on the Internet is an untrustworthy source, unless we get
some evidence of its trustworthiness (which includes at least [a strong
assertion of the source's
identity](#verifying-the-trustworthiness-of-a-source)). When we can know with
certainty that an input is coming from the same source as the application itself
(e.g. Google in the case of Chrome, or Mozilla in the case of Firefox), and that
the transport is integrity-protected (such as with HTTPS), then it can be
acceptable to parse even complex inputs from that source. It's still ideal,
where feasible, to reduce our degree of trust in the source — such as by parsing
the input in a sandbox.

### Unsafe Implementation Languages

_Unsafe implementation languages_ are languages that lack [memory
safety](https://en.wikipedia.org/wiki/Memory_safety), including at least C, C++,
and assembly language. Memory-safe languages include Go, Rust, Python, Java,
JavaScript, Kotlin, and Swift. (Note that the safe subsets of these languages
are safe by design, but of course implementation quality is a different story.)

#### Unsafe Code in Safe Languages

Some memory-safe languages provide a backdoor to unsafety, such as the `unsafe`
keyword in Rust. This functions as a separate unsafe language subset inside the
memory-safe one.

The presence of unsafe code does not negate the memory-safety properties of the
memory-safe language around it as a whole, but _how_ unsafe code is used is
critical. Poor use of an unsafe language subset is not meaningfully different
from any other unsafe implementation language.

In order for a library with unsafe code to be safe for the purposes of the Rule
of 2, all unsafe usage must be able to be reviewed and verified by humans with
simple local reasoning. To achieve this, we expect all unsafe usage to be:
* Small: The minimal possible amount of code to perform the required task
* Encapsulated: All access to the unsafe code is through a safe API
* Documented: All preconditions of an unsafe block (e.g. a call to an unsafe
  function) are spelled out in comments, along with explanations of how they are
  satisfied.

Because unsafe code reaches outside the normal expectations of a memory-safe
language, it must follow strict rules to avoid undefined behaviour and
memory-safety violations, and these are not always easy to verify. A careful
review by one or more experts in the unsafe language subset is required.

It should be safe to use any code in a memory-safe language in a high-privilege
context. As such, the requirements on a memory-safe language implementation are
higher: All code in a memory-safe language must be capable of satisfying the
Rule of 2 in a high-privilege context (including any unsafe code) in order to be
used or admitted anywhere in the project.

### High Privilege

_High privilege_ is a relative term. The very highest-privilege programs are the
computer's firmware, the bootloader, the kernel, any hypervisor or virtual
machine monitor, and so on. Below that are processes that run as an OS-level
account representing a person; this includes the Chrome Browser process and Gpu
process. We consider such processes to have high privilege. (After all, they
can do anything the person can do, with any and all of the person's valuable
data and accounts.)

Processes with slightly reduced privilege will (hopefully soon) include the
network process. These are still pretty high-privilege processes. We are always
looking for ways to reduce their privilege without breaking them.

Low-privilege processes include sandboxed utility processes and renderer
processes with [Site Isolation](
https://www.chromium.org/Home/chromium-security/site-isolation) (very good) or
[origin isolation](
https://cloud.google.com/docs/chrome-enterprise/policies/?policy=IsolateOrigins)
(even better).

### Processing, Parsing, And Deserializing

Turning a stream of bytes into a structured object is hard to do correctly and
safely. For example, turning a stream of bytes into a sequence of Unicode code
points, and from there into an HTML DOM tree with all its elements, attributes,
and metadata, is very error-prone. The same is true of QUIC packets, video
frames, and so on.

Whenever the code branches on the byte values it's processing, the risk
increases that an attacker can influence control flow and exploit bugs in the
implementation.

Although we are all human and mistakes are always possible, a function that does
not branch on input values has a better chance of being free of vulnerabilities.
(Consider an arithmetic function, such as SHA-256, for example.)

## Solutions To This Puzzle

Chrome Security Team will generally not approve landing a CL or new feature
that involves all 3 of untrustworthy inputs, unsafe language, and high
privilege. To solve this problem, you need to get rid of at least 1 of those 3
things. Here are some ways to do that.

### Safe Languages

Where possible, it's great to use a memory-safe language. The following
memory-safe languages are approved for use in Chromium:
* Java (on Android only)
* Swift (on iOS only)
* [Rust](../docs/rust.md) (for [third-party use](
  ../docs/adding_to_third_party.md#Rust))
* JavaScript or WebAssembly (although we don't currently use them in
  high-privilege processes like the browser/gpu process)

One can imagine Kotlin on Android, too, although it is not currently
used in Chromium.

For an example of image processing, we have the pure-Java class
[BaseGifImage](https://cs.chromium.org/chromium/src/third_party/gif_player/src/jp/tomorrowkey/android/gifplayer/BaseGifImage.java?rcl=27febd503d1bab047d73df26db83184fff8d6620&l=27).
On Android, where we can use Java and also face a particularly high cost for
creating new processes (necessary for sandboxing), using Java to decode tricky
formats can be a great approach. We do a similar thing with the pure-Java
[JsonSanitizer](https://cs.chromium.org/chromium/src/services/data_decoder/public/cpp/android/java/src/org/chromium/services/data_decoder/JsonSanitizer.java),
to 'vet' incoming JSON in a memory-safe way before passing the input to the C++
JSON implementation.

On Android, many system APIs that are exposed via Java are not actually
implemented in a safe language, and are instead just facades around an unsafe
implementation. A canonical example of this is the
[BitmapFactory](https://developer.android.com/reference/android/graphics/BitmapFactory)
class, which is a Java wrapper [around C++
Skia](https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/hwui/jni/BitmapFactory.cpp;l=586;drc=864d304156d1ef8985ee39c3c1858349b133b365).
These APIs are therefore not considered memory-safe under the rule.

The [QR code generator](
https://source.chromium.org/chromium/chromium/src/+/main:components/qr_code_generator/;l=1;drc=b185db5d502d4995627e09d62c6934590031a5f2)
is an example of a cross-platform memory-safe Rust library in use in Chromium.

### Privilege Reduction

Also known as [_sandboxing_](https://cs.chromium.org/chromium/src/sandbox/),
privilege reduction means running the code in a process that has had some or
many of its privileges revoked.

When appropriate, try to handle the inputs in a renderer process that is Site
Isolated to the same site as the inputs come from. Take care to validate the
parsed (processed) inputs in the browser, since only the browser can trust
itself to validate and act on the meaning of an object.

Equivalently, you can launch a sandboxed utility process to handle the data, and
return a well-formed response back to the caller in an IPC message. See [Safe
Browsing's ZIP
analyzer](https://cs.chromium.org/chromium/src/chrome/common/safe_browsing/zip_analyzer.h)
for an example. The [Data Decoder Service](https://source.chromium.org/chromium/chromium/src/+/main:services/data_decoder/public/cpp/data_decoder.h)
facilitates this safe decoding process for several common data formats.

### Verifying The Trustworthiness Of A Source

If you can be sure that the input comes from a trustworthy source, it can be OK
to parse/evaluate it at high privilege in an unsafe language. A "trustworthy
source" means that Chromium can cryptographically prove that the data comes
from a business entity that you can or do trust (e.g.
for Chrome, an [Alphabet](https://abc.xyz) company).

Such cryptographic proof can potentially be obtained by:

  * Component Updater;
  * The variations framework.
  * Pinned TLS (see below).

Pinned TLS needs to meet all these criteria to be effective:

  * communication happens via validly-authenticated TLS, HTTPS, or QUIC;
  * the peer's keys are [pinned in Chrome](https://cs.chromium.org/chromium/src/net/http/transport_security_state_static.json?sq=package:chromium&g=0); and
  * pinning is active on all platforms where the feature will launch.
    (Currently pinning is not enabled in iOS or Android WebView).

It is generally preferred to use Component Updater if possible because pinning
may be disabled by locally installed root certificates.

One common pattern is to deliver a cryptographic hash of some content via such
a trustworthy channel, but deliver the content itself via an untrustworthy
channel. So long as the hash is properly verified, that's fine.

### Normalization {#normalization}

You can 'defang' a potentially-malicious input by transforming it into a
_normal_ or _minimal_ form, usually by first transforming it into a format with
a simpler grammar. We say that all data, file, and wire formats are defined by a
_grammar_, even if that grammar is implicit or only partially-specified (as is
so often the case). A data format with a particularly simple grammar is
[`SkPixmap`](https://source.chromium.org/chromium/chromium/src/+/3df9ac8e76132c586e888d1ddc7d2217574f17b0:third_party/skia/include/core/SkPixmap.h;l=712).
(The 'grammar' is represented by the private data fields: a region of raw pixel
data, the size of that region, and simple metadata (`SkImageInfo`) about how to
interpret the pixels.)

It's rare to find such a simple grammar for input formats, however.

For example, consider the PNG image format, which is complex and whose [C
implementation has suffered from memory corruption bugs in the
past](https://www.cvedetails.com/vulnerability-list/vendor_id-7294/Libpng.html).
An attacker could craft a malicious PNG to trigger such a bug. But if you
transform the image into a format that doesn't have PNG's complexity (in a
low-privilege process, of course), the malicious nature of the PNG 'should' be
eliminated and then safe for parsing at a higher privilege level. Even if the
attacker manages to compromise the low-privilege process with a malicious PNG,
the high-privilege process will only parse the compromised process' output with
a simple, plausibly-safe parser. If that parse is successful, the
higher-privilege process can then optionally further transform it into a
normalized, minimal form (such as to save space). Otherwise, the parse can fail
safely, without memory corruption.

The trick of this technique lies in finding a sufficiently-trivial grammar, and
committing to its limitations.

Another good approach is to

  1. define a new Mojo message type for the information you want;
  2. extract that information from a complex input object in a sandboxed
     process; and then
  3. send the result to a higher-privileged process in a Mojo message using the
     new message type.

That way, the higher-privileged process need only process objects adhering to a
well-defined, generally low-complexity grammar. This is a big part of why [we
like for Mojo messages to use structured types](mojo.md#Use-structured-types).

For example, it should be safe enough to convert a PNG to an `SkBitmap` in a
sandboxed process, and then send the `SkBitmap` to a higher-privileged process
via IPC. Although there may be bugs in the IPC message deserialization code
and/or in Skia's `SkBitmap` handling code, we consider this safe enough for a
few reasons:

  * we must accept the risk of bugs in Mojo deserialization; but thankfully
  * Mojo deserialization is very amenable to fuzzing; and
  * it's a big improvement to scope bugs to smaller areas, like IPC
    deserialization functions and very simple classes like `SkBitmap` and
    `SkPixmap`.

Ultimately this process results in parsing significantly simpler grammars. (PNG
→ Mojo + `SkBitmap` in this case.)

> (We have to accept the risk of memory safety bugs in Mojo deserialization
> because C++'s high performance is crucial in such a throughput- and
> latency-sensitive area. If we could change this code to be both in a safer
> language and still have such high performance, that'd be ideal. But that's
> unlikely to happen soon.)

### Exception: Protobuf

While less preferable to Mojo, we also similarly trust Protobuf for
deserializing messages at high privilege from potentially untrustworthy senders.
For example, Protobufs are sometimes embedded in Mojo IPC messages. It is
always preferable to use a Mojo message where possible, though sometimes
external constraints require the use of Protobuf.

Protobuf's threat model does not include parsing a protobuf from shared
memory. Always copy the proto buffer bytes from untrustworthy shared
memory regions before deserializing to a Message.

If you must pass protobuf bytes over mojo use
[mojo_base::ProtoWrapper](https://chromium.googlesource.com/chromium/src/+/main/mojo/public/cpp/base/proto_wrapper.h)
as this provides limited type safety for the top-level protobuf message and
ensures copies are taken before deserializing.

Note that this exception only applies to Protobuf as a container format;
complex data contained within a Protobuf must be handled according to this
rule as well.

### Exception: RE2

As another special case, we trust the
[RE2](https://cs.chromium.org/chromium/src/third_party/re2/README.chromium)
regular expression library to evaluate untrustworthy patterns over untrustworthy
input strings, because its grammar is sufficiently limited and hostile input is
part of the threat model against which it's been tested for years. It is **not**
the case, however, that text matched by an RE2 regular expression is necessarily
"sanitized" or "safe". That requires additional security judgment.

## Safe Types

As discussed above in [Normalization](#normalization), there are some types that
are considered "safe," even though they are deserialized from an untrustworthy
source, at high privilege, and in an unsafe language. These types are
fundamental for passing data between processes using IPC, tend to have simpler
grammar or structure, and/or have been audited or fuzzed heavily.

* `GURL` and `url::Origin`
* `SkBitmap` (in [N32 format](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/include/core/SkColorType.h;l=54-58;drc=8d399817282e3c12ed54eb23ec42a5e418298ec6) only)
* `SkPixmap` (in [N32 format](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/include/core/SkColorType.h;l=54-58;drc=8d399817282e3c12ed54eb23ec42a5e418298ec6) only)
* Protocol buffers (see above; this is not a preferred option and should be
  avoided where possible)

There are also classes in `//base` that internally hold simple values that
represent potentially complex data, such as:

* `base::FilePath`
* `base::Token` and `base::UnguessableToken`
* `base::Time` and `base::TimeDelta`

The deserialization of these is safe, though it is important to remember that
the value itself is still untrustworthy (e.g. a malicious path trying to escape
its parent using `../`).

## Existing Code That Violates The Rule

We still have code that violates this rule.  For example, Chrome's Omnibox
[still parses JSON in the browser
process](https://bugs.chromium.org/p/chromium/issues/detail?id=863193&q=%22rule%20of%202%22%20omnibox&can=1).
Additionally, the networking process on Windows is (at present) unsandboxed by
default, though there is [ongoing
work](https://bugs.chromium.org/p/chromium/issues/detail?id=841001)
to change that default.
