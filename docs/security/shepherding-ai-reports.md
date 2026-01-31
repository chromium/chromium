# Shepherding AI Reports

Chrome Security is getting an increasing amount of vulnerability reports that
are either partly or entirely AI-generated. These reports can be tough to
shepherd because they are often nonsense, and even when they aren't nonsense the
vital information about the bug is buried in a pile of AI-generated words.

Since AI-assisted reports are often speculative, you should also consult
[the speculative bug triage guide](https://goto.google.com/chrome-speculative-bug-triage).

## Spotting AI Reports

There are a few tells for spotting AI reports. Some of these can have false
negatives so seeing one of these tells doesn't necessarily provide ironclad
proof that a report is generated using an AI, but it should make you very
suspicious.

* Bogus external references:
    * Reference to a CVE (like "this bug is similar to CVE-YYYY-ABCDE" or
      similar) where the CVE isn't relevant to the bug report at all
    * References to sections of specs / docs / etc. that don't exist
* Obviously bogus PoCs:
    * PoCs that call APIs that don't exist but plausibly might exist or have
      existed in the past
    * PoCs that are extremely overcomplicated or verbose
    * PoCs that contain no actual code
    * PoCs that claim a crash but do not include a stack trace or ASAN report
    * PoCs (often patches to the browser) that directly call functions not
      usually exposed to an attacker
* Hallucinated technical details - class / function / file names that don't
  exist, stack traces that don't reflect possible call stacks, etc
* Explanations of the theory of an exploit but not an actual exploit:
    * A long explanation of potential impact
    * An explanation of a bug that could exist, but no actual example of it
    * An example of a way that a function could be called unsafely, but not an example of it being called unsafely in Chromium
    * Explanations of hypothetical design weaknesses, especially when very
      long-winded
* Explanations of reasoning or research process which any human researcher would take as understood
* Heavy use of emojis - LLMs tend to embed these especially in code for some
  reason
* References to much older versions of Chrome; generally LLMs are using public
  exploit information so many reports, even if potentially valid, are related to
  long since patched issues
* Prolific reporting from a single reporter across multiple areas within a very
  short timespan (dozens of reports per day in some cases)

## Triaging AI Reports

Just because a reporter used AI to prepare a report does not automatically mean
the report is invalid, but to avoid sinking a lot of time into reports which
have a high probability of being invalid, you should be extra aggressive when
triaging them, and you can generally treat them as lower priority to triage than
reports which look human-written and high-quality. In particular, when triaging
an AI report:

* Skim the report looking for the crux of the actual bug (i.e., skip over all
  the fluff about impact or theory) - you can more or less just skip to code
  sections and ignore the prose for now.
* Quickly check any external references (CVE numbers, etc) for validity
* Eyeball but don't run (yet) the PoC to see if it looks plausible
* If there's a stack trace, check with code search whether it's superficially
  reasonable

Don't bother doing detailed analysis of any AI report that doesn't have a simple
PoC which looks like it could work, or a stack trace which looks valid. In
particular, be very skeptical of AI reports claiming overflows, UaFs, etc that
contain prose explanations of how to reach those conditions - AIs will invent,
and then plausibly lie about, execution traces that lead to vulnerabilities but
that aren't actually possible in practice. Never take at face value a claim from
an AI report that a vulnerability is reachable unless it contains a PoC or an
ASAN stack trace. Feel free to WontFix such reports out of hand and spend your
time on more valuable things.

If you do conclude that a bug is both AI-written and worth WontFixing, please
reference the [FAQ entry on AI bugs][faq-wontfix] as part of your WontFix
message, to encourage reporters to file better bugs.

[faq-wontfix]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md#Should-I-ask-an-AI-to-Generate-a-Vulnerability-Report-for-Chrome

