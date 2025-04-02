# Finding somebody who knows how a piece of code works

It can be more efficient to ask somebody how something works than to take weeks
to figure it out yourself. Likewise, when making changes, you will need to reach
out to someone for review of your code or your designs. So how do you find the
best folks to reach out to? Here are some ideas to get you started.

1. **Ask your teammates** - those who have been on the project longer than you
   will have a better idea or may have contacts in other areas.
1. **Look at the `OWNERS` files** - If you can identify a source file involved
   in what you want to do, find the lowest level
   [`OWNERS` file](code_reviews.md#owners-files) above it, and email those
   people. The code review tool can automatedly find relevant owners for your
   changes, largely based on `OWNERS` files.
1. **Look at the `DIR_METADATA` file** - similar to `OWNERS`,
   [`DIR_METADATA` files](https://chromium.googlesource.com/infra/infra/+/HEAD/go/src/infra/tools/dirmd/README.md)
   contain pointers to relevant experts. Look for the `team_email` field.
1. **Tools** - `git blame` (also available on
   [Chromium code search](https://cs.chromium.org)) and
   [Chromite Butler](https://chrome.google.com/webstore/detail/chromite-butler/bhcnanendmgjjeghamaccjnochlnhcgj)
   can help you find who touched the code last or most extensively. Sometimes
   not just the _author_ but _reviewers_ of a past change will be relevant.
1. **Use the Slack channel**
   ([link](https://www.chromium.org/developers/slack/)) - while not everybody is
   listening all the time, you may get a pointer to the right folks to talk to.
1. **Use the chromium-dev@chromium.org email alias** to ask questions. This is a
   fairly large email list, so use your judgment about what to post, and it may
   take awhile to get a reply. If you do this, it helps to ask one crisp
   question at the top of your mail and then explain in detail instead of
   rambling and asking many questions.

Sometimes, there isn't one person who knows what you want to find out, but you
can find people who know parts of the system. For instance, no one person knows
everything about how Chromium starts up or shuts down and can tell you what you
did wrong in a shutdown scenario, but you can find people who know some parts of
the shutdown to guide you.
