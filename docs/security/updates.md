# Chrome Security Update FAQ

_Bookmark this page as https://g.co/chrome/security-update-faq_

## TL:DR

Almost all Chrome updates contain security fixes, and should be prioritized
equally. The most secure option is to automatically update Chrome as soon as any
update is available, independent of the specific details of any security fixes
included in the update.

## Chrome Updates

Almost all updates have security fixes. Every week, Chrome releases a new
version that materially improves security. The Chrome Security team believes the
best option for security is to apply all updates to Chrome as soon as they are
available. We recommend against and do not support using the security release
notes as a method to prioritize updates. Instead, all updates should be
prioritized equally, and considered to be important security fixes.

Patching, remediating, and protecting against security vulnerabilities is a core
part of the Chrome engineering process. The Chrome Security team works to
protect Chrome users and make it safe to click on links by building features and
architecting systems to defend against exploitation, network tampering, malware
and phishing. Every release of Chrome includes not just security patches, but
new security features and other defensive improvements developed by the Chrome
Security team.

Chrome releases [stable milestones][release-cycle] every four weeks (MXXX), with “refresh”
releases in-between milestones. Milestones are refreshed with updates that can
contain security fixes and functional bug fixes for high-impact bugs. There are
planned security refreshes weekly for every milestone, with all of the
security fixes since the previous release. Chrome may have unscheduled updates
between milestones to fix critical security issues, address breaking functional
bugs, or patch known in-the-wild exploits. Updates and rapid response are part
of the Chrome security development lifecycle, and Chrome invests in its ability
to quickly refresh a release when needed. While we do try to reduce the number
of unplanned updates, these releases are an important part of how Chrome works
to secure its users, and are an example of Chrome’s vulnerability reporting and
security engineering processes working as intended.

## FAQ

**Are security fixes included in every new milestone release?** Yes.

**How do I avoid compatibility issues with web platform changes if I automatically
update Chrome?** Breaking changes to the web platform are communicated in the
[enterprise release notes][ent-rel-notes]. For changes that may cause
compatibility issues, Chrome includes a temporary enterprise policy to maintain
the old behavior for several milestones beyond the change, which gives
enterprises time to address compatibility issues. To proactively identify these
issues, we recommend enterprises test their critical workflows, or have a cohort
of users running the Beta version of Chrome.

You can read more details and best practices on the [Chrome Update Management
Strategies][update-strategy] technical doc.  Enterprises can also reach out to their [support
representatives][ent-support] for help, or [file an issue][issue-tracker] if they identify a
bug in a new version of Chrome.

**How do I prioritize updates that patch vulnerabilities known to be under
exploitation in the wild (zero-days)?** Don't---this strategy puts you at risk.
Security fixes are important regardless of whether or not there is known
exploitation. There are two main reasons for this:

First, we know that we do not have full visibility into every exploit in the
wild. Chrome has an industry-leading multi-layered approach to finding possible
exploits, including code reviews, testing and fuzzing, working with researchers
and external organizations, and a vulnerability rewards program. However, it's
impossible to guarantee that a fixed vulnerability was never exploited in the
wild, so you should always roll out the fix.

Second, vulnerabilities may be exploited after our fix is rolled out. Exploiting
patched vulnerabilities in unpatched installations is known as N-day
exploitation, and it gets easier and cheaper for attackers after we’ve made a
fix available in a new version. If you don't update, you're putting your
organization at risk of N-day exploits, which are much more accessible to bad
actors than 0-day exploits.

Remember, a bug is only regarded as a security bug if we consider it
exploitable, so we recommend Chrome users treat all security fixes with equal
priority and always patch their installation. The [best defense][cisa-patches] against attackers
exploiting patched vulnerabilities in Chrome is to automatically update Chrome
whenever an update is available.

**I've noticed that Chrome has a lot of security updates. Does that mean something
is wrong?** No. Fast and frequent updates are one of Chrome's security strengths.
We've spent years building advanced release infrastructure in order to increase
the number of security updates you receive, and the frequency that we can send
them out. This keeps you a step ahead of bad actors.

**Updating every week (or more) is a lot of work. How can my IT department
keep up?** Chrome is designed to be easy for IT admins to manage and update. If
you're finding that it's a lot of work to keep up with Chrome's update schedule,
there's a good chance that there's an easier way to accomplish your goals.
Enterprises that disable auto-updates and instead roll out manual updates are
both decreasing their security and greatly increasing the amount of work they
have to do.

Read through different update strategies and best practices on the [Chrome Update
Management Strategies][update-strategy] technical doc.

**Is it possible that a Chrome update could address a functional bug, and no
security bugs?** Yes, there are releases containing only functional fixes. When
this occurs, there are no security fix notes for the corresponding desktop stable
release.

**How do I know what security fixes are included in a specific Android Chrome
release?** Android releases contain the same security fixes as their
corresponding desktop release (first three segments of the version number are
the same), unless otherwise noted.

**How do I know what security fixes are included in a specific Chrome on iOS
release?** Due to Apple platform limitations, the browsing engine for Chrome on
iOS is Webkit, which is also used by Safari. Apple maintains Webkit and tracks
security issues as part of their iOS release process. To receive WebKit security
fixes, users should always update iOS to the latest version. Whenever possible,
Chrome will ship patches on iOS to mitigate issues in Webkit. In the event of a
Chrome on iOS specific security flaw, Chrome will note the fix in the release
notes for the corresponding desktop release.

**My product includes one of Chrome's components (such as V8 or WebRTC). How
can I absorb security fixes?** As with Chrome itself, you should assume that
all security fixes are important and arrange to include them in your product.
We recommend tracking Chrome's "stable" branch - more recent branches may have
a higher density of ephemeral security bugs which we will fix before shipping
to stable. For some of Chrome's components (such as WebRTC) security bugs are
relatively infrequent and it may be possible to manually absorb new versions
as necessary by monitoring Chrome release notes. For others (such as V8)
security bugs are so frequent that the better strategy is to absorb a new
version of the component every week. For these components, Chrome does not
support making update decisions based upon the exact security content of the
release - instead, just take a new version each week.

[release-cycle]: https://chromium.googlesource.com/chromium/src/+/main/docs/process/release_cycle.md
[ent-rel-notes]: https://support.google.com/chrome/a/answer/7679408
[update-strategy]: https://support.google.com/chrome/a/answer/9982578
[ent-support]: https://chromeenterprise.google/browser/support/
[issue-tracker]: https://issues.chromium.org
[cisa-patches]: https://www.cisa.gov/tips/st04-006
[chrome-versions]: https://www.chromium.org/developers/version-numbers/
[rel-dash]: https://chromiumdash.appspot.com/releases?platform=Windows
