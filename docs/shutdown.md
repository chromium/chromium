# Chrome Shutdown

[TOC]

This documents shutdown steps on Windows, Mac and Linux.

On Android, the system can terminate the Chrome app at any point without running
any shutdown step.

See below for how the process differs on ChromeOS.

## Step 1: Exiting the main loop

Shutdown starts when nothing keeps Chrome alive. Typically, this happens when
all browser windows are closed, but other things can [keep Chrome
alive](https://source.chromium.org/chromium/chromium/src/+/master:components/keep_alive_registry/keep_alive_types.h).

When nothing keeps Chrome alive, `BrowserProcessImpl::Unpin` asks the main
thread's message loop to quit as soon as it no longer has tasks ready to run
immediately.

```
base::RunLoop::QuitWhenIdle
…
BrowserProcessImpl::Unpin
BrowserProcessImpl::OnKeepAliveStateChanged
KeepAliveRegistry::OnKeepAliveStateChanged
KeepAliveRegistry::Unregister
ScopedKeepAlive::~ScopedKeepAlive
...
Browser::UnregisterKeepAlive
BrowserList::RemoveBrowser
Browser::~Browser
```

Following this request, `ChromeBrowserMainParts::MainMessageLoopRun` exits. Tasks
posted to the main thread without a delay prior to this point are guaranteed to
have run; tasks posted to the main thread after this point will never run.

## Step 2: Cleaning up, after main loop exit

`BrowserMainRunnerImpl::Shutdown` is called on the main thread. Within that
method, `BrowserMainLoop::ShutdownThreadsAndCleanUp` orchestrates the main
shutdown steps.

`ChromeBrowserMainParts::PostMainMessageLoopRun` is invoked. It invokes the
`PostMainMessageLoopRun` method of each `ChromeBrowserMainExtraParts` instance.
This is a good place to perform shutdown steps of a component that require the
IO thread, the `ThreadPool` or the `Profile` to still be available.

`ChromeBrowserMainParts::PostMainMessageLoopRun` also invokes
`BrowserProcessImpl::StartTearDown` which deletes many services owned by
`BrowserProcessImpl` (aka `g_browser_process`). One of these services is the
`ProfileManager`. Deleting the `ProfileManager` deletes `Profiles`. As part of
deleting a `Profile`, its `KeyedServices` are deleted, including:

* Sync Service
* History Service

## Step 3: Joining other threads

The IO thread is joined. No IPC or Mojo can be received after this.

`ThreadPool` shutdown starts. At this point, no new `SKIP_ON_SHUTDOWN` or
`CONTINUE_ON_SHUTDOWN` task can start running (they are deleted without
running). The main thread blocks until all `SKIP_ON_SHUTDOWN` tasks that started
running prior to `ThreadPool` shutdown start are complete, and all
`BLOCK_SHUTDOWN` tasks are complete (irrespective of whether they were posted
before or after `ThreadPool` shutdown start). When no more `SKIP_ON_SHUTDOWN` is
running and no more `BLOCK_SHUTDOWN` task is queued or running, the main thread
is unblocked and `ThreadPool` shutdown is considered complete. Note:
`CONTINUE_ON_SHUTDOWN` tasks that started before `ThreadPool` shutdown may still
be running.

At this point, new tasks posted to the IO thread or to the `ThreadPool` cannot
run. It is illegal to post a `BLOCK_SHUTDOWN` task to the `ThreadPool` (enforced
by a `DCHECK`).

## Step 4: Cleaning up, after joining other threads

`ChromeBrowserMainParts::PostDestroyThreads` is invoked. It invokes
`BrowserProcessImpl::PostDestroyThreads`. Since it is guaranteed that no
`SKIP_ON_SHUTDOWN` or `BLOCK_SHUTDOWN` task is running at this point, it is a
good place to delete objects accessed directly from these tasks.

Then, if a new Chrome executable, it is swapped with the current one
(Windows-only).

```
upgrade_util::SwapNewChromeExeIfPresent
browser_shutdown::ShutdownPostThreadsStop
ChromeBrowserMainParts::PostDestroyThreads
content::BrowserMainLoop::ShutdownThreadsAndCleanUp
content::BrowserMainLoop::ShutdownThreadsAndCleanUp
content::BrowserMainRunnerImpl::Shutdown
```

## ChromeOS differences
On ChromeOS, the ash browser is only supposed to exit when the user logs out.

When the user logs out, the browser sends a `StopSession` message to the
[session_manager](https://chromium.googlesource.com/chromiumos/platform2/+/refs/heads/main/login_manager/README.md).
The session_manager then sends a SIGTERM to the main browser process to cause an
exit. Once SIGTERM is received, it starts shutting down the main loop and
cleaning up in the sequence described above.

Unlike other desktop platforms, the shutdown is time limited. If the browser
process has not exited within a certain time frame (normally, 3 seconds), the
session_manager will SIGKILL the browser process since the user is looking at
a blank screen and unable to use their Chromebook until the browser exits.
