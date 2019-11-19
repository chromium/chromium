# Speed Domains

This organization of Speed into domain areas allows us to:

 * Utilize domain expertise to manage incoming bugs and regression reports
 * Have a clear escalation paths
 * Organize optimization efforts as needed


## Memory

This domain deals with memory use by Chrome on all platforms. Primary focus is to
gracefully deal with situations where user is out of memory (OOM) and to manage
memory for idle and backgrounded tabs.

 * [Mailing List](https://groups.google.com/a/chromium.org/forum/#!forum/memory-dev)
 * Performance-Memory [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DMemory)
 * [Docs](https://chromium.googlesource.com/chromium/src/+/master/docs/memory)

## Power

The Power domain is concerned with improving power usage for our users.
Collectively, our product has an impact on global greenhouse gas emissions and
we would like to mitigate that. Increasing battery life, reducing users power
bill, not burning laps/hands, and not making loud fan noises are all important
benefits of this.

 * Primary Contact brucedawson@chromium.org
 * Power
   [Rotation](https://rotation.googleplex.com/#rotation?id=5428142711767040)
 * Rotation
   [Documentation](https://docs.google.com/document/d/1YgsRvJOi7eJWCTh2p7dy2Wf4EJtjk_3XU30yp_7mhaM/preview)
 * Power
   [Backlog](https://docs.google.com/spreadsheets/d/1VhU1aM6APdUN74NVPW98X3aqpQyJkxg1UJcvBidXaK8/edit)
 * Performance-Power [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DPower)

## Loading

The Loading domain focuses on the time between click to the time when you can
interact with a website.

 * [Mailing
   List](https://groups.google.com/a/chromium.org/forum/#!forum/loading-dev)
 * Performance-Loading [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DLoading)

## Responsiveness

Responsiveness domain focuses on making sure all websites have smooth transitions
by serving 60fps, and that the click to action time is not noticible.

 * Performance-Responsiveness [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DResponsiveness)

## Binary Size

Chrome has an update for you at least every six weeks. Since we do that for all
of our users, we want to be nice to our users where downloading updates costs
real money. We also don't want to hog all of the disk space on low end phones.
So we focus attention on making sure we don't include bits in our update that
are not necessary for users.

 * [Mailing List](://groups.google.com/a/chromium.org/forum/#!forum/binary-size)
 * Performance-Size [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DSize)
 * Description of [metrics](binary_size/metrics.md)

## Data Usage

Data Usage is a focus on the question: Do the user see or need every byte
downloaded? By looking at this, we can save user's cost of data, time to load,
memory and power.

 * Performance-Data [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DData)

## Startup, Omnibox, Browser UI, etc.

There are a handful of performance angles that don't fit into the domains already
mentioned. Historically, we've put these into a "Browser" bucket as that's
descriptive of what's left over. These are things like making sure the Omnibox
experience on Chrome is fast, making sure all of the Chrome UI, e.g. Settings,
is fast and that the browser startup and session restore doesn't allow users to
make coffee before they use the browser.

 * Performance-Browser [Bug
   Queue](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DBrowser)
