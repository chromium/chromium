# Top security things for Chromies to remember

## Why:

1. **This is not a rehearsal**. Exploit brokers trade in Chrome security bugs and
they're used by [foreign governments in harmful ways](https://blog.google/threat-analysis-group/countering-threats-north-korea/).
2. **Security bugs are not theoretical bugs**: If it's marked as a security bug,
the security team believes it's exploitable in practice. Attackers have
incredible toolkits to turn niche, timing-dependent, obscure conditions into
powerful primitives. Unfortunately, they don't need a reliably reproducing bug.
If you can't see a way forward, talk to us.
3. **Attackers deliver data via malicious websites**. Be paranoid when you're
handling data delivered from a website. Do not make any assumptions about its
format or contents. Code defensively. If you're talking to a lower privilege
process (e.g. a renderer process) from a higher privilege process (e.g.
browser) assume it's compromised and is sending you crafted data.
4. **Chromium C++ object lifetimes are often too complicated for human brains**.
You will introduce use-after-free and data race bugs (readily exploitable).
Assume the worst and protect against errors using sandboxes, CHECKs and
fuzzing. If your object lifetimes depend on JavaScript or website data, keep
ownership very simple (avoid raw pointers, avoid `base::Unretained`, single
ownership wherever possible).

## Do's:
1. **Get in touch** ([security@chromium.org](mailto:security@chromium.org)). We
are here to help, and we are humans. We won't give you questionnaires or forms
to fill out, and we will do everything we can to help get you to a secure
outcome.
2. **Use checked
[numerics](https://chromium.googlesource.com/chromium/src/+/HEAD/base/numerics/)
and use C++
[containers](https://chromium.googlesource.com/chromium/src/+/HEAD/base/containers/)**
(Chromium, absl or STL are all OK - they're hardened, or soon-to-be). Prefer
[`base::span`](https://chromium.googlesource.com/chromium/src/+/HEAD/base/containers/span.h#155)
over pointer arithmetic.
3. **Merge fixes with great urgency**. [You fixed a
bug?](security-issue-guide-for-devs.md) Thank you! It's now
easier to exploit. N-day attackers are now weaponizing your publicly-visible
git commit. Work urgently with the security and release TPMs to handle merges,
to get the fix out to users before n-day exploitation gets widespread. Step
one: simply mark the bug as Fixed, and then sheriffbot and the TPMs will get in
touch to ask for the correct merges.
4. **Keep things simple**. If your object lifetimes confuse you, or if your
data structure can be modified in three places, or if you have to keep two data
structures synchronized at all times, you've probably got a security bug.
5. **Intentionally crash**: it's better than being exploited. `DCHECK`s don't
prevent security incidents, because release builds don't have them, so use
`CHECK`s unless calculating the condition is expensive.
6. **Write fuzzers** for any data that your code is ingesting. It can be [just
a few lines of code](../../testing/libfuzzer/getting_started.md)
and it can be mesmerizing to see the fuzzer explore your code.

## Don'ts:

1. **Don't use `base::Unretained`** without a comment explaining how you can
prove the object lifetimes are safe - it's responsible for a high percentage of
our exploitable bugs. The best alternative? Consider `SafeRef<T>` or
`WeakPtr<T>` - or ask us!
2. **Don't run unnecessary code in the browser process**: it has all the
secrets. Run the code elsewhere if you can. If it must run in the browser
process, be extra paranoid.
3. **Don't mess with security UX**. Humankind vaguely knows to trust UI drawn
by the browser, but not to trust UI drawn by the website. Don't blur the lines.
This awareness is [like a fragile
butterfly](web-platform-security-guidelines.md#security-ux). [URL display is
equally delicate](url_display_guidelines/url_display_guidelines.md).

For more information, see our security [guidelines](rules.md), [FAQ](faq.md)
and [suggestions for what to do if you get a security
bug](security-issue-guide-for-devs.md). And don't hesitate to contact
[security@chromium.org](mailto:security@chromium.org) - we want to help. Thanks
for reading!

