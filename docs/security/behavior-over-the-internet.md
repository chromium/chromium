# Guidelines for delivery of Chrome behavior over the Internet

**Summary**: It's OK to deliver _content_ to Chrome dynamically over the internet,
but Chrome behavior should be a part of the Chrome binary or delivered via
Component Updater.

There are several reasons for this:

* TLS is insufficient to prove categorically that the behavior comes from the
  Chrome organization (due primarily to the risk that malware, enterprise
  security software, or governments have arranged to install extra TLS root
  certificates). So you'd need to build a custom signature verification scheme.
  This is hard (per the mantra "don't roll your own crypto"). The code is
  fiddly; you'd need to sustain processes for key rotations, revocation,
  detecting private key compromise etc.
* Even if the transport is secure, a compromised server could show fake Chrome
  UI and spoof websites.
* Users expect Chrome behavior to be part of the Chrome download, and are
  surprised when it dynamically changes. Chrome does support dynamically
  changing behavior in several ways, such as the Variations framework. But
  these mechanisms are well-documented and controlled by e.g. enterprise
  policies, so it's better to use one of them than re-inventing the wheel
  -- and they conveniently support out-of-band signing. Dynamic behavior
  also adds a combinatorial factor which makes it harder to test or fuzz
  Chrome, or to put in place simple reliable security defenses which apply
  universally.

What do we mean by 'behavior'? 'Behavior' is anything that a reasonable user
would consider to be 'part of Chrome' as opposed to content displayed by
Chrome. So, the feed of articles is definitely 'content' whereas the settings
pages are 'behavior'. Web pages themselves are 'content' but any action which
the browser applies to those pages is 'behavior'. Generally speaking, code
which runs in the browser process is likely to be 'behavior'. Configuration
information is likely to be 'behavior' too if it meaningfully alters Chrome's
functionality.

A solution is available for fairly-dynamic Chrome behavior:
[Component Updater](https://chromium.googlesource.com/chromium/src/+/main/components/component_updater/README.md).
Components can be updated without an update to Chrome itself, which allows
them to have faster or desynchronized release cadences. (The same [signing
technology](https://g3doc.corp.google.com/company/teams/chrome/intelligence/serving_on_device_models.md?cl=head)
can be used elsewhere, but this should be avoided if possible
because component updater has documentation and controls such as enterprise
policies that might need to be replicated for per-feature signing and code
delivery schemes).
