# Sandbox FAQ

[TOC]

### What is the sandbox?

The sandbox is a C++ library that allows the creation of sandboxed processes —
processes that execute within a very restrictive environment. The only resources
sandboxed processes can freely use are CPU cycles and memory. For example,
sandboxes processes cannot write to disk or display their own windows. What
exactly they can do is controlled by an explicit policy. Chromium renderers are
sandboxed processes, meaning they operate in an isolated environment with very limited
resources. The exact capabilities of these processes are governed by an explicit policy,
which determines the premissions and access rights of each process.

This architecture ensures that even if a vulnerabilitiy is exploited in a web page's
rendering process, the impact is confined to the sandbox and does not compromise the use's
system. This is part of Chromium's multi-process architecture, which aims to improve
the browser's overall security and stability. When a web page is loaded in the browser,
the task of rendering the page is handled by these sandboxed renderer processes.

### What does and doesn't it protect against?

The sandbox limits the severity of bugs in code running inside the sandbox. Such
bugs cannot install persistent malware in the user's account (because writing to
the filesystem is banned). Such bugs also cannot read and steal arbitrary files
from the user's machine.

(In Chromium, the renderer processes are sandboxed and have this
protection. After the NPAPI removal, all remaining plugins are also
sandboxed. Also note that Chromium renderer processes are isolated from the
system, but not yet from the web. Therefore, domain-based data isolation is not
yet provided.).

The sandbox cannot provide any protection against bugs in system components such
as the kernel it is running on.

### Is the sandbox like what you get with the Java VM?

Yeah, kind of... except that to take advantage of the Java sandbox, you must
rewrite your code to use Java. With our sandbox you can add sandboxing to your
existing C/C++ applications. Because the code is not executed inside a virtual
machine, you get native speed and direct access to the Windows API.

### Do I need to install a driver or kernel module? Does the user need to be Administrator?

No and no. The sandbox is a pure user-mode library, and any user can run
sandboxed processes.

### How can you do this for C++ code if there is no virtual machine?

We leverage the Windows security model. In Windows, code cannot perform any form
of I/O (be it disk, keyboard, or screen) without making a system call. In most
system calls, Windows performs some sort of security check. The sandbox sets
things up so that these security checks fail for the kinds of actions that you
don’t want the sandboxed process to perform. In Chromium, the sandbox is such
that all access checks should fail.

### So how can a sandboxed process such as a renderer accomplish anything?

Certain communication channels are explicitly open for the sandboxed processes;
the processes can write and read from these channels. A more privileged process
can use these channels to do certain actions on behalf of the sandboxed
process. In Chromium, the privileged process is usually the browser process.

### Doesn't Vista have similar functionality? 

Yes. It's called integrity levels (ILs). The sandbox detects Vista and uses
integrity levels, as well. The main difference is that the sandbox also works
well under Windows XP. The only application that we are aware of that uses ILs
is Internet Explorer 7. In other words, leveraging the new Vista security
features is one of the things that the sandbox library does for you.

### This is very neat. Can I use the sandbox in my own programs?

Yes. The sandbox does not have any hard dependencies on the Chromium browser and
was designed to be used with other Internet-facing applications. The main hurdle
is that you have to split your application into at least two interacting
processes. One of the processes is privileged and does I/O and interacts with
the user; the other is not privileged at all and does untrusted data processing.

### Isn't that a lot of work?

Possibly. But it's worth the trouble, especially if your application processes
arbitrary untrusted data. Any buffer overflow or format decoding flaw that your
code might have won't automatically result in malicious code compromising the
whole computer. The sandbox is not a security silver bullet, but it is a strong
last defense against nasty exploits.

### Should I be aware of any gotchas?

Well, the main thing to keep in mind is that you should only sandbox code that
you fully control or that you fully understand. Sandboxing third-party code can
be very difficult. For example, you might not be aware of third-party code's
need to create temporary files or display warning dialogs; these operations will
not succeed unless you explicitly allow them. Furthermore, third-party
components could get updated on the end-user machine with new behaviors that you
did not anticipate.

### How about COM, Winsock, or DirectX — can I use them?

For the most part, no. We recommend against using them before lock-down. Once a
sandboxed process is locked down, use of Winsock, COM, or DirectX will either
malfunction or outright fail.

### What do you mean by before _lock-down_? Is the sandboxed process not locked down from the start?

No, the sandboxed process does not start fully secured. The sandbox takes effect
once the process makes a call to the sandbox method `LowerToken()`. This allows
for a period during process startup when the sandboxed process can freely get
hold of critical resources, load libraries, or read configuration files. The
process should call `LowerToken()` as soon as feasible and certainly before it
starts interacting with untrusted data.

**Note:** Any OS handle that is left open after calling `LowerToken()` can be
abused by malware if your process gets infected. That's why we discourage
calling COM or other heavyweight APIs; they leave some open handles around for
efficiency in later calls.

### So what APIs can you call?

There is no master list of safe APIs. In general, structure your code such that
the sandboxed code reads and writes from pipes or shared memory and just does
operations over this data. In the case of Chromium, the entire WebKit code runs
this way, and the output is mostly a rendered bitmap of the web pages. You can
use Chromium as inspiration for your own memory- or pipe-based IPC.

### But can't malware just infect the process at the other end of the pipe or shared memory?

Yes, it might, if there's a bug there. The key point is that it's easier to
write and analyze a correct IPC mechanism than, say, a web browser
engine. Strive to make the IPC code as simple as possible, and have it reviewed
by others.
