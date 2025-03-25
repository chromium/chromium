# Gerrit Guide

[TOC]

## Introduction

### (EVERYONE) To get access to the Chromium Gerrit instance

1. Install
   [depot_tools](https://www.chromium.org/developers/how-tos/install-depot-tools/).

2. Set up your account on Gerrit by visiting
   https://chromium-review.googlesource.com/ and signing in once. This makes
   sure that you have an account on these review systems, which is required for
   uploading new CLs.

3. To ensure that git cl workflows authenticate correctly:

        git config --global depot-tools.useNewAuthStack 1

4. If you are using a @google.com account:

    1. Run gcert once a day to authenticate your account.

5. Make sure to set your real name.

    1. Visit https://chromium-review.googlesource.com/#/settings/ and check the
       "Full Name" field.

### (Googler) Link @chromium.org & @google.com accounts

If you have both @chromium.org and @google.com accounts, you may want to link
them. Doing so may make it easier to view all of your CLs at once, and may make
it less likely that you'll upload a CL with the wrong account.

However, if you do choose to link them, you will be prompted to log in using
only your @google.com account, and that means you have to follow all of the
normal security restrictions for such accounts.

**Please note** that linking your accounts does NOT change ownership of CLs
you've already uploaded and you will **lose edit access** to any CLs owned by
your secondary (@google.com) account. i.e CLs you uploaded with your @google.com
account, before the link, will not show up in your @chromium.org dashboard. Any
in-flight changes will have to be re-uploaded, so if you have significant
in-flight changes we don't recommend linking accounts.

**To link them:**

==If you have two email accounts== (@chromium.org and @google.com) but only have
one Gerrit account you can link them yourself:

1. Login into https://chromium-review.googlesource.com using your @chromium.org account.
2. Go to [Settings -> Email Addresses](https://chromium-review.googlesource.com/#/settings/EmailAddresses).
3. In the "New email address" field, enter your @google.com account, click the
   Send Verification button, and follow the instructions.

    1. If you see an error on clicking the link, use this link to file a ticket go/fix-chrome-git

4. To verify that it worked, open [Settings ->
   Identities](https://chromium-review.googlesource.com/#/settings/web-identities)
   and verify your @chromium.org, @google.com and ldapuser/* identities are
   listed.
5. Repeat 1-4 on https://chrome-internal-review.googlesource.com, but use your
   @google.com email to login, and @chromium.org in "Register new email" dialog.
6. If you see any errors during this process, file a Infra-Git ticket with the
   subject "Link my <id>@chromium.org and <id>@google.com accounts". If it is
   urgent, email ajp@chromium.org. Otherwise, the request should be handled
   within 2-3 days.

==If you have two Gerrit accounts== you need an admin to link them. File a
Infra-Git ticket with the subject "Link my <id>@chromium.org and <id>@google.com
accounts". If it is urgent, email ajp@chromium.org. Otherwise, the request
should be handled within 2-3 days.

Once your accounts are linked, you'll be able to use both @chromium.org and
@google.com emails in git commits. It is particularly useful if you have your
@chromium.org email in global git config, and you try to trigger chrome-internal
trybots (that otherwise require @google.com email).

If you have linked accounts, and want to unlink them:

* On chromium-review, go to
  https://chromium-review.googlesource.com/settings/#EmailAddresses, click
  "Delete" on all addresses associated with the account you don't want to use
  anymore (e.g. all the @google ones), and then sign in again using the account
  you do want to use (e.g. their @chromium one).
* On chrome-internal-review, go to
  https://chrome-internal-review.googlesource.com/settings/#EmailAddresses and
  do the same (probably deleting @chromium, and then signing in with your
  @google account).

If you see any errors during this process, file [Infra-Git
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
with the subject "Unlink my <id>@chromium.org and <id>@google.com accounts". If
it is urgent, email ajp@chromium.org. Otherwise, the request should be handled
within 2-3 days.

### (EVERYONE) Verification

Run `git ls-remote https://chromium.googlesource.com/chromiumos/manifest.git`

This should **not** prompt for any credentials, and should just print out a list
of git references.

## Watching Projects / Notifications

You can select Projects (and branches) you want to "watch" for any changes on by
adding the Project under [Settings ->
Notifications](https://chromium-review.googlesource.com/settings/#Notifications).

## How do I build on other ongoing Gerrit reviews?

Scenario: You have an ongoing Gerrit review, with issue number 123456 (this is
the number after the last / in the URL for your Gerrit review). You have a local
branch, with your change, say 2a40ae.

Someone else has an ongoing Gerrit review, with issue number 456789. You want to
build on this. Here’s one way to do it:

```
git checkout -b their_branch

git cl patch -f 456789

git checkout -b my_branch # yes, create a new

git cherry-pick 2a40ae # your change from local branch

git branch --set-upstream-to=their_branch

git rebase

git cl issue 123456

<any more changes to your commit(s)>

git cl upload
```

## Not getting email?

In case you think you should be receiving email from Gerrit but don't see it in
your inbox, be sure to check your spam folder. It's possible that your mail
reader is mis-classifying email from Gerrit as spam.

## Still having a problem?

Check out the [Gerrit
Documentation](https://gerrit-review.googlesource.com/Documentation/index.html)
to see if there are hints in there.

If you have any problems please [open a Build Infrastructure
issue](https://bugs.chromium.org/p/chromium/issues/entry?template=Build+Infrastructure)
on the **Chromium** issue tracker (the "Build Infrastructure" template should be
automatically selected).

For additional information, you can also visit the [PolyGerrit + Chromium
FAQ](https://polygerrit.appspot.com/).
