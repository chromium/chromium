# Inclusive Chromium code

## Why this is important

Our [Code of Conduct](https://www.chromium.org/conduct) under "Be respectful and
constructive" says:

> Each of us has the right to __enjoy our experience__ and participate without
> fear of harassment, __discrimination__, or __condescension__, whether blatant
> or subtle.

Emphasis is added: unnecessarily exclusive code is discriminatory and
condescending, and reading biased code isn't enjoyable.

## Gender-Neutral

Some points in our code, documentation and comments contain needless assumptions
about the gender of a future reader, user, etc. __Example: "When the user logs
into his profile."__

### Suggestions on how to keep code gender-neutral

These are only suggestions. You make the call.

Things to avoid:

* Gendered pronouns: he / she / him / her / his / hers, etc.
* Instances of the phrases "he or she", "his/hers", "(s)he", etc. All of these
  still exclude those who don't identify with either gender, and implicitly
  (slightly) favor one gender via listing it first.
* "Guys" as a gender-neutral term, which has male associations. Usually in
  comments it implies anthropomorphism of inanimate objects and should be
  replaced with a more precise technical term. If it does refer to people,
  consider using "everyone", "folks", "people", "peeps", "y'all", etc.
* Other gendered words: "brother", "mother", "man", etc.

Cases that are likely fine to leave alone include:

* References to a specific person ("Rachel is on leave; update this when she is
  back.").
* A name ("Guy" and "He" are both valid names).
* A language code ("he" is the ISO 639-1 language code for Hebrew).
* He as an abbreviation for "helium".
* The Spanish word "he".
* References to a specific fictional person ([Alice, Bob, ...](http://en.wikipedia.org/wiki/Alice_and_Bob)).
  * For new code/comments, consider using just 'A', 'B' as names.
* Quotations and content of things like public-domain books.
* Partner agreements and legal documents we can no longer edit.
* Occurrences in randomly generated strings or base-64 encodings.
* Content in a language other than English unless you are fluent in that
  language.

How to change the remaining awkward intrusions of gender:

* Try rewording things to not involve a pronoun at all. In many cases this makes
  the documentation clearer. Example: "I tell him when I am all done." → "I tell
  the owner when I am all done." This saves the reader a tiny bit of mental
  pointer-dereferencing.
* Try using [singular they](https://en.wikipedia.org/wiki/Singular_they).
* Try making hypothetical people plural. "When the user is done he'll
  probably..."
→ "When users complete this step, they probably...".
* When referring to a non-person, "it" or "one" may be good alternatives ([wikipedia link](http://wikipedia.org/wiki/Gender-specific_and_gender-neutral_pronouns#It_and_one_as_gender-neutral_pronouns)).

### Example changelists

For a long list of changes, see [this bug](https://crbug.com/542537). Some
examples:

* [Make some code gender neutral](https://crrev.com/e3496e19094cf7e711508fe69b197aa13725c790)
* [Updates comments in the src files to remove gender specific terms](https://crrev.com/5b9d52e8a6ec9c11a675bae21c7d1b0448689fb6)
* [Gender-neutralize a few more comments / strings](https://crrev.com/993006d919c7b0d3e2309041c1e45b8d7fc7ff0e)
* [Make some android code gender neutral](https://crrev.com/93d83ac96c3d1c27be9ea7e842b25b3dded2550b)

### Tools

* [Here](https://cs.chromium.org/search/?q=%28%5E%7C%5Cs%7C%5C%28%7C%5C%5B%29%28%5BHh%5De%7C%5BSs%5Dhe%7C%5BHh%5Dis%7C%5BHh%5Ders?%7C%5BHh%5Dim%7C%5BHh%5Der%7C%5BGg%5Duys?%29%5Cb&sq=package:chromium&type=cs)
  is a code search link prepopulated with a search that finds a lot of gendered
  terms.
* To search for files containing gendered terms, use this command (or a variant
  of it):
```
git grep -wiEIl \ '(he)|(she)|(his)|(hers)|(him)|(her)|(guy)|(guys)'
```
* To search in a file open in vim for gendered terms, use this command:
```
/\<he\>\|\<she\>\|\<his\>\|\<hers\>\|\<him\>\|\<her\>\|\<guy\>\|\<guys\>|\<man\>\c
```
* There are presubmit checks available for this that are run for the infra and
  v8 repos. They are not run for other repos as there are too many false
  positives.

## Racially neutral

Terms such as "blacklist" and "whitelist" reinforce the notion that black==bad
and white==good. [That Word *Black*, by Langston
Hughes](https://mcwriting11.blogspot.com/2014/06/that-word-black-by-langston-hughes.html)
illustrates this problem in a lighthearted, if somewhat pointed way.

These terms can usually be replaced by "blocklist" and "allowlist" without
changing their meanings, but particular instances may need other replacements.

### Example changelists

For a long list of changes, see [this bug](https://crbug.com/842296). Some
examples:

* ["Blacklist"->"Blocklist" in interventions-internals UI.](https://crrev.com/c/1055905)
* [Remove "whitelist" and "blacklist" from extension docs.](https://crrev.com/c/1056027)
* [Declarative Net Request: Replace usages of 'blacklist' and 'whitelist'.](https://crrev.com/c/1094141)

## Thanks

This document is based on an internal Google document by Rachel Grey and others,
which can be found [here](https://goto.google.com/gender-neutral-code) (sorry,
Googlers only).
