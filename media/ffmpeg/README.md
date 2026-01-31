# //media/ffmpeg

This directory contains all of the [scripts](scripts/) for rolling FFmpeg in
Chrome, as well as all of the glue code for interfacing with ffmpeg, such
as the [FFmpegDecodingLoop](ffmpeg_decoding_loop.h) and
[ScopedAVPacket](scoped_av_packet.h) files.

## FFmpeg Tips & Tricks

### Performing a Roll

Detailed instructions for rolling FFmpeg live in the tree at
[robosushi.md](scripts/robosushi.md).

### Upstreaming Your Patch(es)

We strongly encourage upstreaming local changes to FFmpeg whenever possible to
minimize the maintenance burden of our local fork.

As of [July 2025](https://ffmpeg.org/pipermail/ffmpeg-devel/2025-July/346938.html),
FFmpeg officially supports two methods for submitting patches. The **Git forge**
is generally recommended for its modern workflow and automated CI feedback.

#### Option 1: code.ffmpeg.org (Recommended)

This is the modern submission workflow, running on a Forgejo instance. It
supports standard Pull Requests and provides immediate CI testing.

1. Create an account at [code.ffmpeg.org](https://code.ffmpeg.org/).
2. Fork the `FFmpeg/FFmpeg` repository.
3. Push your changes to your fork and open a Pull Request.

#### Option 2: ffmpeg-devel Mailing List

This is the traditional submission method. It is still fully supported and
widely used by long-time contributors,.

1. Subscribe to the [ffmpeg-devel](https://ffmpeg.org/mailman/listinfo/ffmpeg-devel)
   mailing list.
2. Format your patches using `git format-patch`.
3. Submit them using `git send-email`. See the documentation below in the
   [Using `git send-email`](#using-git-send-email) section.

#### Non-Option 3: GitHub

Do _not_ open Pull Requests on the
[GitHub mirror](https://github.com/FFmpeg/FFmpeg). That repository is
read-only and PRs submitted there are ignored.

#### Using `git send-email`

For more information, see the
[FFmpeg docs on submitting patches](https://ffmpeg.p2hp.com/developer.html#toc-Submitting-patches-1).
More in depth steps are provided below.

##### Step 1: Create a new app

Visit [app-passwords](https://myaccount.google.com/apppasswords)
and create a new app named `git-send-email`.

##### Step 2: Save your password

**SAVE THIS PASSWORD. You can't view it again once you hide it.**

##### Step 3: Update your git configuration

Set up your git configuration - in `~/.gitconfig`. You should include the
following lines:

```ini
   [sendemail]
     smtpencryption = tls
     smtpserver = smtp.gmail.com
     smtpuser = [username]@chromium.org
     smtpserverport = 587
```

**`@google.com` addresses should work fine as well.**

You _can_ save the password here using `smtppass = aaaa bbbb cccc dddd`, but
take care to ensure that this password remains private.

##### Step 4: Checkout upstream

Check out a totally fresh, upstream copy of ffmpeg:

```bash
git clone https://git.ffmpeg.org/ffmpeg.git
```

##### Step 5: Prepare your patch for submission

Apply your patch, commit it, and then follow the upstream instructions for
testing and formatting your patch in the
[FFmpeg developer documentation](https://ffmpeg.org/developer.html#Submitting-patches-1).

##### Step 6: Send your patch

Send your patch. This will ask you for the password from steps 1&2.

```bash
git send-email --confirm=auto --to=ffmpeg-devel@ffmpeg.org --annotate HEAD^
```
