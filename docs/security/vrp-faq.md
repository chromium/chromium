# Chrome Vulnerability Reward Program (VRP) News and FAQ

[TOC]

## News and Updates

Please report all Chromium security bugs in the new tracker using [this
form](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342)
or https://bughunters.google.com/report/vrp -> Chrome VRP.

Please check here for any news and updates about the Chrome VRP.

* 20 June 2024: All Google VRPs, including **Chrome VRP, have a new payments
  processing option through Bugcrowd**. To use the Bugcrowd option to receive
  your Chrome VRP reward payments, you must:
    * Set up your profile in bughunters.google.com and associate it with the
      email address you use to report issues in chromium.issues.com.
    * Be registered or [register with](https://bugcrowd.com/user/sign_up)
      Bugcrowd.
    * In your [Bughunters profile](https://bughunters.google.com/profile),
      select `Bugcrowd` under `Payment Options` and enter the email address for
      your Bugcrowd account.
    * Hit `Save` on your profile and you're ready to roll!

* 4 February 2024: The Chromium issue tracker migration is now complete. Please
  submit all issues using the [new issue tracker](https://issues.chromium.org)
  and use [this
  form](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342)
  for directly reporting security issue to the security team.

* 25 January 2024: We have updated our VRP policy on duplicates and collisions
  for actionable versus non-actionable reports. Please see the [Chrome VRP
  qualifying vulnerabilities](https://g.co/chrome/vrp/#qualifying-vulnerabilities)
  for details.

* December 2023: We announced the [Chrome VRP Top Researchers of
  2023](https://crbug.com/1509898). Congratulations to all who made the list!
  Thank you to all of our researchers for your contributions in 2023 and helping
  us make Chrome Browser more secure for all users.

## About the Program

The Chrome Vulnerability Rewards Program (VRP) is the "security bug bounty" for
Google Chrome Browser. Please visit the [Chrome VRP Rewards and
Policies](https://g.co/chrome/vrp) page for full details.

### Interesting Security Bug Write-Ups

Here are some interesting write-ups of past Chrome security bugs:

* [A Bug's Life: CVE-2021-21225](https://tiszka.com/blog/CVE_2021_21225.html)
* [Exploiting CVE-2021-21225 and disabling
  W^X](https://tiszka.com/blog/CVE_2021_21225_exploit.html)
* [ZIP embedding attack on Google Chrome
  extension](https://readme.synack.com/exploits-explained-zip-embedding-attack-on-google-chrome-extensions),
  by Malcolm Stagg, reporter of CVE-2024-0333

We only post links to articles with the author's consent. Please let us know if
you would like your work to be shared here.

## Best Practices for Security Bug Reporting

To help make the process of security bug triage as efficient and smooth as
possible, please consider the following best practices for Chromium security bug
reports:

* Use the [security bug reporting
  form](https://issues.chromium.org/issues/new?noWizard=true&component=1363614&template=1922342)
  * This will allow the bug report to be included in the security bug triage
    queue immediately.
* Include the version number and OS used to reproduce the bug. For an [extra
  bonus reward](https://g.co/chrome/vrp/#bisect-bonus), please consider
  including a bisection.

### Proof of Concept (PoC)

* Upload PoC files directly to the report itself as separate attachments.
* Please name the PoC file *poc.html* or *index.html* in the case of multiple
  files.
  * This allows [Security Shepherds](shepherd.md) to run the PoC in ClusterFuzz
  immediately for triage.
* Ensure the PoC is as minimized as possible.
  * This is useful in root cause analysis and also improves the changes of
    reproduction by ClusterFuzz or the Security Shepherd triaging your report.
* Please format your PoCs so they do not require running python as root.
  * Your PoC should be constructed to reproduce locally when at all possible.
* Do *not* provide links (public or unlisted) to / as PoCs.
  * The PoC should ALWAYS be a file directly attached to the report, even if it
    cannot be reproduced in ClusterFuzz.

### Steps to Reproduce

* Please include clear, concise, numbered steps to reproduce the bug you are
  reporting.
* Provide the full command line and any build flags to reproduce.

### Report Attachments

* Do *not* upload attachments as a single compressed file, such as .zip,
  .gzip, or .tar files.
  * Please upload all necessary attachments (PoCs, patch, fix patches, video
    demonstrations, exploits) as individual files on the bug report.
* ASAN stack traces should be symbolized, and include all `additional
  information` fields, and the sanitizer should be built using Chrome's standard
  mitigation and hardening flags.
* Patch.diff files that are not suggested fixes should explain - as part of the
  steps to repro - the reason for the patch, such as to simulate a compromised
  renderer or allow for stable reproduction, such as running the PoC in a loop
  or winning a race condition.
* Do not provide links (public or unlisted) to demonstration videos.
  * If possible, please upload your demonstration video directly to the report.
  * Please keep the video as concise as possible.
    * We do not need to see you loading Chrome or displaying the version page
      multiple times in a video.
  * DO feel free to include music, sound effects, or animal pics in your
    videos. :)

### Bug Reporting

* Please ensure your report is concise and clearly articulates the bug,
  explaining and demonstrating (through PoC, exploit, and/or video), such as:
  * How the issue is reachable and exploitable by web content or a compromised
    renderer.
  * The security consequences / user harm resulting from exploitation of the
    bug.
* Avoid reporting theoretical bugs; reports that simply state a potential bug
  from static analysis without demonstration of the security issue.
  * These reports will be considered as unactionable and will be triaged at a
    lower priority, and may be closed as WontFix without demonstrable evidence
    of a security bug.
  * These reports are also considered below baseline quality and qualify for
    reduced or no VRP reward.
* At a minimum, please include the version number and release channel
  (Stable/Beta/Dev) on which you discovered and reproduced the issue you are
  reporting.
  * For an opportunity at increasing your potential reward amount to receive a
    [Bisect Bonus](https://g.co/chrome/vrp/#bisect-bonus),
    please consider performing a full bisection, detailing the commit that
    introduced the issue and / or all the active release channels impacted by
    the bug.

### Suggested Fix / Patch Rewards

* If suggesting a patch to fix the issue you are reporting, please upload it to
  the report as its own attachment.
* We reward [bonuses for your patches](https://g.co/chrome/vrp/#patch-bonus)
  that end up being used as the fix.
  * Bonuses are $500 - $2000 depending on how substantial the patch is.
  * To maximize your patch rewards, please commit the patch directly to Chromium
    and include the Gerrit (Chromium code review tool) link in the report or
    report comment.
  * Remember to include the Chromium bug tracker issue number in the CL, if you
    land the patch after filing the bug report, so that it can be linked to the
    report.

## Frequently Asked Questions (FAQ)

### Scope / Reward Eligibility

#### How do I know if my bug report is possibly eligible for a VRP reward?

* All validated, [qualifying vulnerability
  reports](https://g.co/chrome/vrp/#qualifying-vulnerabilities) are
  automatically considered for a reward once they are fixed. At which point you
  will see the reward-topanel hotlist signifier added to your bug report. This
  indicates that it will be reviewed at a Chrome VRP panel meeting for a
  reward decision.
* The bug will be updated again once the panel has made a reward decision.

#### I want to report a bug through a broker / not directly to you.

* We believe it is against the spirit of the program to privately disclose
  security vulnerabilities to third parties for purposes other than fixing the
  bug. Consequently, such reports will not qualify for a reward.

#### What if someone else reported the same bug?

* Only the first actionable report of a security bug that we were previously
  unaware of is eligible for a potential VRP reward. In the event of a duplicate
  submission, the earliest actionable report in the tracker is considered the
  first report.
* If the issue is discovered by one of our internal fuzzers within 48 hours of
  your report, it is considered a known issue and is not eligible for a reward.

#### What does actionable report mean?

* A security bug report is only considered actionable once it contains
  appropriate information that allows for validation and triage, such as a
  minimized test case / PoC, steps to reproduce, a symbolized stack trace,
  and/or other demonstrable evidence of the security bug being reported.
* A report lacking this actionable information and does not allow us to
  reproduce the issue to validate and investigate the bug is not considered to
  be a complete, actionable submission.

#### Are bugs in unlaunched features / behind command line flags VRP-eligible?

* Yes, we are interested in bugs in any code that has shipped to even a fraction
  of our users.
* *The only exception at this time are security bugs in V8 behind
  --experimental*; this flag is for early and experimental V8 development
  efforts and this configuration should not be used in production. *Security
  bugs specific to this configuration are not eligible for Chrome VRP rewards.*
* Please ensure you include what command line flags are required to trigger the
  bug in your report and remove any unnecessary flags.
* The report will be triaged with the appropriate security
  severity and as Security_Impact-None. This categorization of
  Security_Impact-None does not impact the potential reward amount.
* Please note that these security bugs do not have the same SLO to fix as bugs
  impacting Stable, Beta, or Dev. The only expectation is that the bug is fixed
  before the feature is exposed to users, such as part of an origin trial or
  field experiment.

#### What about bugs in channels other than Stable?

* We are interested in security bugs in Stable, Beta, and Dev channels because
  it's best for everyone to find and fix security bugs before they impact Stable
  channel users.
* We do, however, discourage hunting in Canary or Trunk builds. These do not go
  through release testing and exhibit short-lived regression that are typically
  identified and fixed quickly. Reports of bugs in new code in trunk may collide
  with on-going work of the engineers as part of "trunk-churn".
* Reports for bugs in newly landed code on Trunk / Head landed within 48 hours
  of the report are not eligible for VRP rewards.
  * VRP eligibility for reports in Head will be based on assessment of ongoing
    development efforts and discussion with the engineering team to determine if
    the VRP report was used in identifying and fixing that issue.
  * Bugs for issues resulting from newly landed commits on Head that are seven
    (7) or fewer days old are likely to not be eligible for a VRP reward.

#### Are bugs in third-party components in scope and eligible for VRP rewards?

* Yes, we are interested in reports for bugs in our third-party components, such
  as libxml, sqlite, and image and compression libraries. The security impact
  must manifest in and result in security consequences in shipped configurations
  of Chrome.
* Reports should demonstrate that the vulnerable code is reachable in Chrome.
* We are interested in rewarding any information that enables us to better
  protect our users. In the event of bugs in an external component (such as an
  upstream dependency), we may also ask that you file a bug directly with the
  vendor or maintainer for that component, so that the product or project owners
  can set about fixing the bug immediately.
* This also ensures that you have direct access to the status of the report, can
  directly communicate with that vendor or project owner, and receive credit or
  acknowledgement (if they have such a mechanism to do so).

#### Can I submit my report(s) and provide a working exploit later?
Is there a time limit for submitting an exploit?

* Most definitely! We realize that developing an exploit is a lengthy process
  and we very much encourage this approach, as it allows us to work on fixing
  the bugs as soon as possible. It also reduces the chance that someone else
  reports the same issue while you are working on the exploit.
* Although we don't have a set time limit, we would expect that the exploit
  would follow within six  weeks of the initial report. If more time is needed,
  we are happy to discuss extended timelines.
  * Please reach out to security-vrp@chromium.org to discuss exploit extensions.

#### Will you reward for types of bugs that are not specifically listed?

* Yes. We are interested in rewarding any information that enables us to better
  protect Chrome users. Reward amounts are based on the potential security harm
  of the bug being reported and the quality of the report.

### Report Status / Bug Lifecycle

#### What happens after I report a security bug to you?

* Please see [Life of a Security Issue](life-of-a-security-issue.md).

#### Will I receive a CVE for my bug?

* If the issue you report was an exploitable security bug impacting Stable or
  older version of Chrome, and it was the first actionable report of that
  issue, your bug will be issued a CVE at the time the fix ships in a Stable
  channel update of Chrome.
* The CVE number will be updated directly on the report itself and listed in
  the Chrome Browser release notes for that Stable channel update.

### Disclosure / Report Visibility

#### What if I disclose the bug publicly before you have fixed or disclosed it?

* Please read [our stance on vulnerability
  disclosure](https://about.google/appsecurity/#:~:text=Google's%20vulnerability%20disclosure%20policy,a%2090%2Dday%20disclosure%20deadline)
* Essentially, our pledge to you is to respond promptly and fix bugs in a
  sensible timeframe, and in exchange, we ask for a reasonable notice of
  potential disclosure.
* Disclosures that go against this principle will usually not qualify for a VRP
  reward, but we will evaluate them on a case-by-case basis.

#### When will the bug I reported be publicly disclosed?

* Most security bugs are automatically opened for public access 14 weeks after
  the bug is closed as Fixed, meaning the fix commit is landed on Chromium main.
* Our automation removes the view restrictions, opening the report for public
  visibility at that time.

#### Can you keep my identity confidential from the public?

* Your email address is obscured by default (by elision) in the Chromium bug
  tracker and is not revealed once the bug is publicly disclosed.
* If you do not want to receive public acknowledgement for your report, please
  let us know in the report or before the issue is listed in the release notes.
  * We will credit the finding to "anonymous" researcher or we are happy to
    credit it to whatever pseudonym or tag you provide to us.
* If you receive a VRP reward for your report and accept it, Google or Bugcrowd
  (depending on who you select to process your VRP reward) will need to
  privately collect some identifying information to process your reward
  payment.

#### Can you keep my report under Security Embargo?

* Security Embargo prevents issues from being disclosed beyond the security team
  and engineers working to resolve the bug. Once the issue is fixed, the
  (Security Notify) community of embedders and developers of other
  Chromium-based products are reliant on the access to bug reports. Because
  Security Embargo restricts this access indefinitely, it is used only on a
  specific case-by-case basis, such as when the bug should never be publicly
  disclosed.

### Rewards / Reward Decisions

#### My bug was rewarded under older amounts, will you pay the difference?

* We often increase reward amounts and introduce new bonus opportunities. We
  reward bug reports based only on the rules that were in effect at the time of
  bug submission and VRP Panel assessment.

#### The exploit market pays more for bugs!

* Yes, we are aware that the exploit brokers, underground markets, three cats in
  a trenchcoat on your local street corner may pay more for bugs. We understand
  that many pockets of society may pay you more money to purchase information
  about the vulnerabilities you may find or exploits you develop. These people
  buy vulnerabilities and exploits for offensive purposes to target people
  using Chrome across the internet. We believe the reward you get under those
  circumstances comes with strings attached -- including buying your silence
  and accepting that any bug you sell may be used to target other people without
  their knowledge, such as activists, dissidents, and vulnerable populations.
* We understand our reward amounts may be less than these alternatives, but we
  also believe we provide additional benefits when reporting directly to us,
  such as public acknowledgement of your findings and skills, the opportunity to
  engage directly with the security team and engineers working to resolve your
  bug, a quick resolution and seeing a fix you contributed to go out into the
  world, the opportunity to publicly discuss/blog/present/share your amazing
  work, and *the knowledge that you are helping keep Chrome secure for billions
  of people across the world*!
* Also, many of our researchers receive gifts of swag and are invited to events.
* You'll additionally have the peace of mind to know your bug findings were
  never used by shady people for nefarious purposes.

#### When will I receive my reward?

* Once the bug has been assessed by the VRP Panel, the bug report is updated
  with a reward decision and information.
* There are two options for payment of a VRP Reward -- direct through Google
  or through Bugcrowd.
  * Through Google:
    * If this is your first VRP reward for a Google program, a member of the
      finance p2p-vrp team will reach out to enroll you in the Google payment
      system.
    * VRP payments are handled by the p2p-vrp finance team. Once you have been
       enrolled, you will receive you payment within 1-2 weeks of a reward
      decision.
    * Please reach out to p2p-vrp@google.com with questions about the payment
      enrollment process or assistance with any payments issues.
  * Through Bugcrowd:
    * You must already have or create a new Google
      [Bughunters](https://bughunters.google.com/profile) profile.
      (Please note, you can set your Bughunters profile to be private if you
      prefer to not have a public profile).
    * Associate your Bughunters profile with the email address you use for
      reporting Chrome security issues.
    * Have a Bugcrowd account or [register](https://bugcrowd.com/user/sign_up)
      with Bugcrowd.
    * In your [Bughunters profile](https://bughunters.google.com/profile)
      change your `Payment Options` from `Legacy` to `Bugcrowd` and enter the
      email address for your Bugcrowd account (and hit `Save`)!
    * Future reward payments will be sent to Bugcrowd for processing and you
      will receive an email directly from Bugcrowd to accept those rewards.
*  If at any point you want to change the method by which you receive VRP reward
   payments, this can be done through your Google Bughunters profile > `Payments
   Options`:
    * Select `Legacy` to receive your payments through Google p2p payments
       processing.
    * Select `Bugcrowd` to select payments through Bugcrowd. Remember you must
      register with Bugcrowd first and enter your Bugcrowd account email in your
      Bughunters profile.

#### I don't agree with the reward amount. Can I get the reward reassessed?

* We always try our best to be fair and consistent, but sometimes we may get it
  wrong or miss something in our assessment. If you feel that is the case,
  please reach out to us at security-vrp@chromium.org detailing why you believe
  we should reassess your report.

### Other

I have a security-related question that is not listed here.

* This is a statement not a question, but we're happy to help. Take a look at
  the [Chrome Security FAQ](faq.md) to see if your question is answered there.
* Also, if you have not already, please check the [Chrome VRP Rewards and
  Policies page](https://g.co/chrome/vrp).
* If you still need assistance, please reach out to security-vrp@chromium.org.


