# Gerrit Guide

[TOC]

## Introduction

### (EVERYONE) To get access to the Chromium Gerrit instance

1. Install
   [depot_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).

2. Set up your account on Gerrit by visiting
   https://chromium-review.googlesource.com/ and signing in once. This makes
   sure that you have an account, which is needed for uploading CLs.

3. Set your real name on Gerrit by visiting
   https://chromium-review.googlesource.com/#/settings/ and check the "Full
   Name" field.

4. Ensure Git is set up correctly:

        # Make sure to set your name and email
        git config --global user.name "CHANGE ME"
        git config --global user.email CHANGE_ME@chromium.org
        git config --global depot-tools.useNewAuthStack 1
        update_depot_tools
        git cl creds-check --global

5. If you are using a @google.com account:

    1. Run gcert once a day to authenticate your account.

### (EVERYONE) Verification

Run `git ls-remote https://chromium.googlesource.com/chromiumos/manifest.git`

This should **not** prompt for any credentials, and should just print out a list
of git references.

### (Googler) Link @chromium.org & @google.com accounts

If you have both @chromium.org and @google.com accounts, you may want to link
them.

Doing so may make it easier to view all of your CLs at once, and may make it
less likely that you'll upload a CL with the wrong account.

However, if you do choose to link them, you will be prompted to log in using
only your @google.com account, and that means you have to follow all of the
security restrictions for @google.com accounts.

**Please note** that linking your accounts does NOT change ownership of CLs
you've already uploaded and **you will lose edit access** to any CLs owned by
your secondary (@google.com) account. i.e CLs you uploaded with your @google.com
account, before the link, will not show up in your @chromium.org dashboard. Any
in-flight changes will have to be re-uploaded, so if you have significant
in-flight changes we don't recommend linking accounts.

**To link them:**

If you have two email accounts (@chromium.org and @google.com) but **only have
one Gerrit account** you can link them yourself:

1. Login into https://chromium-review.googlesource.com using your @chromium.org account.
2. Go to [Settings -> Email Addresses](https://chromium-review.googlesource.com/#/settings/EmailAddresses).
3. In the "New email address" field, enter your @google.com account, click the
   Send Verification button, and follow the instructions.
4. To verify that it worked, open [Settings ->
   Identities](https://chromium-review.googlesource.com/#/settings/web-identities)
   and verify your @chromium.org, @google.com and ldapuser/* identities are
   listed.
5. Repeat 1-4 on https://chrome-internal-review.googlesource.com, but use your
   @google.com email to login, and @chromium.org in "Register new email" dialog.

If you encounter any errors, [file a ticket](https://issues.chromium.org/issues/new?component=1456263&template=1923295).

**If you have two Gerrit accounts** you need an admin to link them. File a
ticket using go/fix-chrome-git

Once your accounts are linked, you'll be able to use both @chromium.org and
@google.com emails in git commits. This is particularly useful if you have your
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

If you encounter any errors, [file a ticket](https://issues.chromium.org/issues/new?component=1456263&template=1923295).

## Common issues

### email address blah@chromium.org is not registered in your account, and you lack 'forge committer' permission

This means that the email you're using to upload CLs is not the same as the
email you're making Git commits with.

To fix this problem, make sure your Git configured email is correct:

    git config --global user.email CHANGE_ME@chromium.org

Run to fix your Gerrit auth:

    git cl creds-check

If you don't use `git cl upload` or if you use it with `--no-squash`, you may
need to rewrite your commits with the correct email:

    git rebase -f

### SSOAuthenticator: Timeout

If you're getting this error and you're using SSH to Windows, try using Chrome
Remote Desktop instead.  (SSH introduces more latency depending on where you're
connecting to/from.)

### Not getting email?

In case you think you should be receiving email from Gerrit but don't see it in
your inbox, be sure to check your spam folder. It's possible that your mail
reader is mis-classifying email from Gerrit as spam.

### Still having a problem?

Check out the [Gerrit
Documentation](https://gerrit-review.googlesource.com/Documentation/index.html)
to see if there are hints in there.

If you have any problems please [open a Build Infrastructure
issue](https://bugs.chromium.org/p/chromium/issues/entry?template=Build+Infrastructure)
on the **Chromium** issue tracker (the "Build Infrastructure" template should be
automatically selected).

## Tips

### Watching Projects / Notifications

You can select Projects (and branches) you want to "watch" for any changes on by
adding the Project under [Settings ->
Notifications](https://chromium-review.googlesource.com/settings/#Notifications).

### How do I build on other ongoing Gerrit reviews?

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
