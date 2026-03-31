# Delegated Triage

For certain areas of Chromium, security bug triage is delegated from the core
security team to other teams. This doc describes how that process works at a
high level and what you (a delegated triager) are supposed to do. This doc is an
abridged version of the [shepherd guide], which describes the full security bug
triage process, including a lot of steps and special cases you do not need to
handle.

## Security Triage

Security bug reports in Chromium need to be _valid_, which means they:

* Describe actual security bugs in Chromium (a defect that a malicious party can
  use to materially harm a Chromium user)
* Have sufficient evidence and detail to allow engineers to reproduce them

For every valid security bug report, our bug-response and release processes then
require some metadata:

* Which versions are affected (so we know where to merge fixes to)
* Which OSes are affected (so we know which platforms need new releases)
* The [severity](severity-guidelines.md) of the bug is (so we know how urgently to do those things)

And then it needs to be assigned to engineers to be fixed, by setting:

* A component (where the root cause of the bug lies)
* A list of CC'd users (because people can't view security bugs by default)
* An assignee (who will actually fix the bug)

## Delegated Triage Process

When a new security bug report is filed in the bug tracker, the on-duty security
triager is normally responsible for performing all the steps above, for every
incoming security bug. For delegated triage rotations, the security triager will
instead simply move matching bugs into a special component for delegated triage,
at which point delegated triagers for that component (that's you!) take over.

If at any point you get stuck, confused, or this process isn't working for you,
you can get in touch with the on-duty security shepherds: [shepherd-1] and
[shepherd-2]. Please do not assign bugs to these shepherds, but you can reach them over Chat.

For each security bug in the component you're triaging, do these steps:

### Skim

The goal of this step is to quickly reject bugs that are probably invalid, so
that the more time-consuming later steps only happen on bugs that are likely to
be valid.

Quickly skim the bug, asking yourself: _could_ this be a valid security bug?

* If it doesn't seem like a valid bug at all, WontFix it and you are done
* If it seems like a duplicate of an existing bug report (the bug tracker will
  surface examples), mark it as a duplicate. Do **not** CC the reporter on the
  canonical bug unless the canonical bug is already public. If they ask to be
  CCed, tell them to email product-security@chromium.org.
* If it does seem like a valid bug but you can't see _clear_ security
  consequences, change its type to Bug and you are done
* If it does seem like a valid bug with clear security consequences, is the
  information you need to reproduce it present? If not, WontFix it with a
  message telling the reporter what info was missing and asking them to re-file
  with the needed info, and you are done
* If all that info appears present, skim the attached proof-of-concept (if
  there is one) or other evidence - does it look at least vaguely plausible? If
  not, WontFix it and you are done

### Repro

The goal of this step is to become _nearly certain_ that the bug is valid and
_fairly confident_ that it has security consequences.

**Never run a proof-of-concept or repro steps that are not obviously safe**.

* If there is a proof-of-concept, do a more detailed reading of it. If it is not
  **obviously safe** for you to run, WontFix the bug and ask the reporter to
  re-file it with a better proof-of-concept. You are done.
* If there is a proof-of-concept, but it doesn't clearly demonstrate the bug
  when you run it, WontFix the bug with an explanation of what happened and ask
  the reporter to re-file with a better proof-of-concept. Note that scary log
  messages like "EXPLOIT CONFIRMED" are not proof of anything; you must see a
  genuine crash, ASAN violation, or some other concrete thing **not** emitted by
  the proof-of-concept itself.
* If there is a video from the reporter, watch it, and if there are repro steps,
  follow them; do you get the same behavior?

If the PoC does work for you, and demonstrates a security problem, leave a
comment on the bug describing what happened, what revision you tested at, what
build config you used, and any other info that someone else might need to
reproduce the bug later. Hooray, you have a valid security bug!

### Annotate

The goal of this step is to attach the metadata needed later in the bug-fixing
process. By now, you are able to reproduce the bug yourself, so use either your
judgment, code-reading, or your ability to reproduce the bug to:

* Find the earliest release starting with the [current extended
  stable](https://chromiumdash.appspot.com/schedule) that
  the bug is present in, and **set FoundIn** to that milestone
* Assess which OSes it applies to, and **set OS**
* Assess the bug's [severity] and **set Severity**; you can check with the
  on-duty security shepherd if you need help, but bear in mind they know less
  about what you're doing than you do

### Assign

The goal of this step is to get the bug to an engineering team that will fix it,
and to ensure that security bugs are always assigned to someone so that we can
stay within our SLOs.

* Find who owns the code where the root cause (or the most proximate cause in
  Chromium) lies. Add all those owners to the CC list of the bug.
* Pick some likely owner and assign the bug directly to them.
* Finally, move the bug into a relevant-looking component, taking it out of your
  own triage queue

Great work, you have triaged a security bug!

[shepherd guide]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/shepherd.md
[shepherd-1]: https://oncall.corp.google.com/interrupts.chrome-security-shepherds-1
[shepherd-2]: https://oncall.corp.google.com/interrupts.chrome-security-shepherds-2
[severity]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md
