# Chrome Remote Desktop Architecture

## Overview

### ME2ME and IT2ME

Remote access (ME2ME) refers to a feature/mode of CRD where:

*   The user sets up Me2Me once on the host machine. After that it continues to
    run as a daemon process, surviving logouts and reboots.
*   Chrome need not be running in order for the host process to be active.
    Specifically, it is possible to connect as soon as the computer has booted
    up, before anyone is logged on.

Remote support (IT2ME) refers to a feature/mode of CRD where:

*   A user is required at the Host computer to "invite" the Viewer computer to
    connect. This is done via a short-lived numerical code displayed at the Host
    and relayed over the phone to the Viewer.
*   Sessions are paused after 30 minutes, at which point the Host user must
    explicitly approve continuation. The Host process runs as a Native Messaging
    binary, with no privileged access to the system.

### Host and client

The `host` is either the machine that is sharing out its screen, or the process
or software that allows for such functionality, depending on the context. Use
terms like `host machine` and `host process` to disambiguate if necessary.

The `client` is the website used to connect to (and remotely control) the host,
or the machine that does that, depending on the context. Use terms like `client
machine` and `client website` to disambiguate if necessary. Note that the code
for the client website **is not in this repo**.

### WebRTC and Chromotocol

CRD currently uses WebRTC for communication between the host and the client.
There is obsolete code in our code base for a communication protocol called
Chromotocol, which was used by deprecated non-website clients, which will soon
be deleted.

### Multi-process architecture

The CRD Windows host has a multi-process architecture, where we run a daemon
process as SYSTEM at MIC SYSTEM level (//remoting/host/daemon_process.h), which
also acts as a mojo IPC broker. A network process
(//remoting/host/remoting_me2me_host.cc) is run as LOCAL_SERVICE at MIC LOW
level for actually communicating with the client via network requests. A third
desktop process (//remoting/host/desktop_process.h) is run as SYSTEM at MIC
SYSTEM level, which is responsible to capturing screen frame data and other
operations that need to be done on the user session. There are also user
processes such as the remote-open-url process and file transfer process, which
is run by the logged in user with MIC MEDIUM level within the CRD session, which
talks to other CRD processes using Mojo IPC. All CRD processes use Mojo IPC to
communicate with each other. Mojo connection is established by connecting to the
broker process, i.e. the daemon process.

The Mac host has an agent process broker process
(//remoting/host/mac/agent_process_broker_main.cc), which is used to make sure
only one host process (i.e. network process, but with screen capturing and all
other logic in it) is run at a time. The agent process broker process also acts
as the mojo broker for user processes. It doesn't have a real multi-process
architecture. Network request handling and screen capturing are done in the same
process.

The official Linux host currently only has one single host process (i.e. network
process), and it acts as the mojo broker for user processes. However, we are
currently working on a true multi-process architecture for Linux. The dev
multi-process host on Linux can be run with
`remoting/tools/run_multi_process_host.py $OUT_DIRECTORY` (e.g.
OUT_DIRECTORY=out/debug) after building `remoting_dev_me2me_host`.

When necessary, read the following files:

*   remoting/host/host_main.cc: Logic to select which routine should be run,
    based on the `--type` switch.
    *   This file defines all entry points for processes without a standalone
        binary. The implementations of the entry points live in other files, so
        you will need to look them up if necessary.
    *   This file also has the logic to determine whether the current process is
        a mojo broker.
*   remoting/host/remoting_me2me_host.cc: Entry point of the network process.
*   remoting/host/daemon_process.h: Class that holds the daemon process logic.
    Note that different platforms have different entry points for the daemon
    process:
    *   remoting/host/win/host_service.cc: The entry point of the daemon process
        on Windows.
    *   remoting/host/linux/daemon_process_main.cc: The entry point of the
        daemon process on Linux.
*   remoting/host/desktop_process_main.cc: Entry point of the desktop process.
*   remoting/host/mojom/desktop_session.mojom: Defines interfaces for
    communications between the network process and the desktop process.
*   remoting/host/mojom/remoting_host.mojom: Defines interfaces for
    communications between the network process and the daemon process.
*   remoting/host/mojom/chromoting_host_services.mojom: Defines interfaces to
    allow user processes to communicate with the network process.
*   Other files in remoting/host/mojom: Interfaces for other purposes. Read when
    necessary.

## Adding/removing processes

There are two types of processes in CRD, namely, processes with a standalone
binary and processes without a standalone binary. If the developer asks you to
add a new process, ask them which type of process they want to add.

### Processes with a standalone binary

These are processes such as remote-open-url and remote-webauthn. To add a new
process with a standalone binary:

1.  Ask if the developer wants to add everything to a new subdirectory or to an
    existing directory (e.g., `remoting/host/linux/`).
1.  Ask which platform(s) the binary should be added to (e.g., Linux only, or
    both Linux and Windows). This ensures you don't add unnecessary
    platform-specific code or configuration for a binary that only runs on a
    subset of platforms.
1.  Create a main function header for the new binary. Make sure the function has
    `REMOTING_EXPORT`. See remoting/host/linux/migrate_host_main.h or
    remoting/host/remote_open_url/remote_open_url_main.h as examples.
1.  Implement the main function. See
    remoting/host/linux/migrate_host_main.cc or
    remoting/host/remote_open_url/remote_open_url_main.cc as examples.
1.  Create an entry point file with the actual `main` function, which calls the
    function in the main function header. See
    remoting/host/linux/migrate_host_entry_point.cc or
    remoting/host/remote_open_url/remote_open_url_entry_point.cc as examples.
1.  Create a `source_set` in a BUILD.gn file for the main header, which depends
    on //remoting/host:host_main_headers, and update the `host_main_headers`
    target in remoting/host/BUILD.gn so that it is visible to the new target.
    See remoting/host/linux/BUILD.gn or
    remoting/host/remote_open_url/BUILD.gn as examples.
1.  The main function implementation may be added to either a new target
    (usually when it is in a new directory), or an existing target (e.g.,
    `platform_impls` in `remoting/host/linux/BUILD.gn`). If it is a new target,
    make sure it has `configs += [
    "//remoting/build/config:host_implementation" ]`. The target should depend
    on the main header target created in the step above. Otherwise the symbol
    won't be exposed. See remoting/host/remote_open_url/BUILD.gn as an example.
1.  Add the executable target, which depends on the main header target and
    remoting_core. See remoting/host/linux/BUILD.gn or
    remoting/host/remote_open_url/BUILD.gn as examples.
1.  If the binary is for Linux, also add it to:
    *   The `deps` list of `group("remoting_dev_me2me_host")` in
        `remoting/host/linux/BUILD.gn`.
    *   The `inputs` and `deps` lists of `action("remoting_me2me_host_deb_installer")`
        in `remoting/host/installer/linux/BUILD.gn`.
    *   The `PROGNAME` variable definition and `install` sections in
        `remoting/host/installer/linux/Makefile`.
1.  If the process needs to be run on Windows, then pay attention to the `if
    (is_win) {...}` section of the example in the step above, and also update
    remoting/host/win/core.rc.jinja2. If it is never run on Windows, then you
    can ignore this step.

### Processes without a standalone binary

These are processes like the daemon process and the network process. They are
run with `$MAIN_HOST_BINARY --type=$PROCESS_TYPE`. For example, you can run the
dev daemon process with `remoting_me2me_host --type=daemon`. To add a new
process without a standalone binary:

1.  Add a new entry point to remoting/host/host_main.cc
1.  Update SelectMainRoutine in remoting/host/host_main.cc
1.  Implement the the entry point in another file that is built into
    remoting_core.

## AI Development Guidance

When working within the `//remoting/host` directory, always refer to the local
`GEMINI.md` files for process-specific navigation and "landmine" warnings:

*   **Main Routing:** `//remoting/host/GEMINI.md`
*   **Windows Implementation:** `//remoting/host/win/GEMINI.md`
*   **Mojo IPC Interfaces:** `//remoting/host/mojom/GEMINI.md`

Use these files to map the `--type` flags to implementation files and to
understand the security constraints of the multi-process architecture.
