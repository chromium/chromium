# How to repro bot failures

If you're looking for repro a CI/CQ compile or test bot failures locally, then
this is the right doc. This doc tends to getting you to the exact same
environment as bot for repro. Most likely you don't need to follow the exact
steps here. But if you have a hard time for repro, then it's good to check each
steps. Also keep in mind that even you have the exact same environment, some
failures are not reproduceable locally.

When trying to repro and fix a bot failure, the easier approach is tryjobs, or
[debug with swarming](../workflow/debugging-with-swarming.md).
This doc is more useful when it requires more local debugging and testing.

This doc assumes you're familiar with Chrome development on at least 1 platform.

## Identify the platform

Usually this is easy by looking at the builder name. e.g. linux-rel means it's
on linux platform.

## Understand the platform system requirements

Usually you should be able to follow
[Chromium Get Code](https://www.chromium.org/developers/how-tos/get-the-code/)
and understand the platform.

## Prepare the compile and test devices

For compile devices, if you see a "compilator steps"(usually for CQ jobs),

![compilator step]

then
click the "compilator build" and then bot on the right.

![bot link]

Otherwise just click bot on right.

For test devices, for most builders, click the shard and then click on bot.

![test shard]

The bot page would look like this

![bot page]

You would want to prepare the exact same environment when possible. e.g.
many tests are using e2-standard-8 GCE bot, you will want to create a
e2-standard-8 GCE VM. If you see python 3.8, then install python 3.8.

## Checkout code

Follow the platform specific workflow to checkout the code.

## Revision

On bot, you can see revision like this.

![revision]

First sync to that revision. For tryjobs, you also need to cherry-pick the
changelist with correct patchset after sync to correct revision.

Note: Don't just download a patch from gerrit to repro tryjob failures.
On bot, it will first checkout the branch HEAD, then patch the patchset.
But on gerrit, what you see is your local revision when you upload with
the patchset. On gerrit you see code at an older base revision.

## Change gclient config
On bot, there is a step named 'gclient config'.

![gclient]

Apply the same gclient config and then rerun 'gclient sync'.

## gn args

On bot, there is a step named 'lookup GN args'.

![gn args]

Apply the same gn args locally.
Note: most of the time, some path based flags can be omitted. Examples:
* coverage_instrumentation_input_file

At the time of writing, some gn args can't be applied locally, like
use_remoteexec.

Note: ChromeOS builders(or any platform that involves 'import' gn rules)
may show complicated gn args. For these builders, you need to search the
builder name in //tools/mb or //internal/tools/mb for internal builders to
find out the gn args.

## Compile

On bot, there is a step named 'compile'. Go to 'execution_details' and compile
all the targets listed in there.

## Run test

Most of the time, on bot the compile device is not the same as the test device.
But locally for desktop platforms, they are the same device and in the same
//out folder. There is no easy way to mimic bot behavior. A common issue on bot
is you see some missing files but can't repro locally. This is because on bot
we calculate what files are needed for testing using build maps and copy them
from compile machine to test machine. But locally you don't have this step.
Usually the fix is to add some 'data' or 'data_deps' in BUILD.gn.

Open the bot test result page

![test page]

You should see all the information you need including system environment and
commandlines.

Note: Set system environment. Especially important for gtest GTEST_SHARD_INDEX
GTEST_TOTAL_SHARDS.

Note: On bot, the CURRENT_DIR is in the //out dir. Say you compile test target
in //out/Default, then enter //out/Default and then run tests.

Note: For gtest, if you're not using the same machine type, you might want to
override some flags like --test-launcher-jobs.

[bot link]: images/bot_repro_bot_link.png
[bot page]: images/bot_repro_bot_page.png
[compilator step]: images/bot_repro_compilator_step.png
[gclient]: images/bot_repro_gclient.png
[gn args]: images/bot_repro_gn_args.png
[revision]: images/bot_repro_revision.png
[test page]: images/bot_repro_test_page.png
[test shard]: images/bot_repro_test_shard.png
