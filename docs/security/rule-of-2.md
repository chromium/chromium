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
[Normalization](#Normalization) for a potential example.) Therefore, we do need
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

### High Privilege

_High privilege_ is a relative term. The very highest-privilege programs are the
computer's firmware, the bootloader, the kernel, any hypervisor or virtual
machine monitor, and so on. Below that are processes that run as an OS-level
account representing a person; this includes the Chrome browser process. We
consider such processes to have high privilege. (After all, they can do anything
the person can do, with any and all of the person's valuable data and accounts.)

Processes with slightly reduced privilege include (as of March 2019) the GPU
process and (hopefully soon) the network process. These are still pretty
high-privilege processes. We are always looking for ways to reduce their
privilege without breaking them.

Low-privilege processes include sandboxed utility processes and renderer
processes with [Site
Isolation](https://www.chromium.org/Home/chromium-security/site-isolation) (very
good) or [origin
isolation](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=IsolateOrigins)
(even better).

## Solutions To This Puzzle

Chrome Security Team will generally not approve landing a CL or new feature
that involves all 3 of untrustworthy inputs, unsafe language, and high
privilege. To solve this problem, you need to get rid of at least 1 of those 3
things. Here are some ways to do that.

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
for an example.

### Verifying The Trustworthiness Of A Source

If you can be sure that the input comes from a trustworthy source, it can be OK
to parse/evaluate it at high privilege in an unsafe language. A "trustworthy
source" meets all of these criteria:

  * communication happens via validly-authenticated TLS, HTTPS, or QUIC;
  * the peer's keys are [pinned in Chrome](https://cs.chromium.org/chromium/src/net/http/transport_security_state_static.json?sq=package:chromium&g=0); and
  * the peer is operated by a business entity that you can or do trust (e.g.
    for Chrome, an [Alphabet](https://abc.xyz) company).

### Normalization {#normalization}

You can 'defang' a potentially-malicious input by transforming it into a
_normal_ or _minimal_ form, usually by first transforming it into a format with
a simpler grammar. We say that all data, file, and wire formats are defined by a
_grammar_, even if that grammar is implicit or only partially-specified (as is
so often the case). A file format with a particularly simple grammar is
[Farbfeld](https://tools.suckless.org/farbfeld/). (The grammar is represented in
the table at the top.)

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

### Safe Languages

Where possible, it's great to use a memory-safe language. Of the currently
approved set of implementation languages in Chromium, the most likely candidates
are Java (on Android only) and JavaScript or WebAssembly (although we don't
currently use them in high-privilege processes like the browser). One can
imagine Swift on iOS or Kotlin on Android, too, although they are not currently
used in Chromium. (Some of us on Security Team aspire to get more of Chromium in
safer languages, but that's a long-term, heavy lift.)

For an example of image processing, we have the pure-Java class
[BaseGifImage](https://cs.chromium.org/chromium/src/third_party/gif_player/src/jp/tomorrowkey/android/gifplayer/BaseGifImage.java?rcl=27febd503d1bab047d73df26db83184fff8d6620&l=27).
On Android, where we can use Java and also face a particularly high cost for
creating new processes (necessary for sandboxing), using Java to decode tricky
formats can be a great approach. We do a similar thing with the pure-Java
[JsonSanitizer](https://cs.chromium.org/chromium/src/services/data_decoder/public/cpp/android/java/src/org/chromium/services/data_decoder/JsonSanitizer.java),
to 'vet' incoming JSON in a memory-safe way before passing the input to the C++
JSON implementation.

## Existing Code That Violates The Rule

We still have a lot of code that violates this rule. For example, until very
recently, all of the network stack was in the browser process, and its whole job
is to parse complex and untrustworthy inputs (TLS, QUIC, HTTP, DNS, X.509, and
more). This dangerous combination is why bugs in that area of code are often of
Critical severity:

  * [OOB Write in `QuicStreamSequencerBuffer::OnStreamData`](https://bugs.chromium.org/p/chromium/issues/detail?id=778505)
  * [Stack Buffer Overflow in `QuicClientPromisedInfo::OnPromiseHeaders`](https://bugs.chromium.org/p/chromium/issues/detail?id=777728)

We now have the network stack in its own dedicated process, and have begun the
process of reducing that process' privilege. ([macOS
bug](https://bugs.chromium.org/p/chromium/issues/detail?id=915910), [Windows
bug](https://bugs.chromium.org/p/chromium/issues/detail?id=841001))
