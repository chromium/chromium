# Debugging Chromium Python With The VSCode Debugger

## Before You Begin

1. Patch in [this CL](https://chromium-review.googlesource.com/c/chromium/src/+/2466896).
2. Run gclient sync.

## Via SSH

SSH is useful if you’re modifying and debugging code on another device, such as
the desktop sitting at your office desk. To do so:

1. Set up VSCode to work with normal development by following the instructions
   in the Remote Visual Studio Code section
   [here](https://docs.google.com/document/d/1ZlG8VQxudxvDs-EtpQvaPVcAPfSMdYlXr42s_487wLo/edit#bookmark=id.j10hyv6nlkws).
2. Open the Connection Dialog of Chrome’s SSH plugin: ![open
   dialog](images/vscode_python_connection_dialog.png)
3. Create a new connection and set the username, hostname, port, and SSH relay
   server options as you normally would. Then, set SSH arguments to "-2 -L
   50371:localhost:50371"

    a. You can replace 50371 with a different value, so long as it's consistent
    with step 7b.

4. Open a connection, and set this window aside.
5. In VSCode, open the code you want to set a breakpoint in, and add the
   following:

```
import debugpy

# Your code here!
debugpy.listen(50371)
print("Wait for attach...")
debugpy.wait_for_attach()
debugpy.brerakpoint()
```

Note: The port passed to debugpy.listen() should match the port configured in (3).

6. Click on the Debug tab
7. Click Run. A dialog will appear asking you to set up a debug configuration.
   Do so, and select “Remote Debug”.

    a. Leave the hostname as-is

    b. Set the port to 50371

8. Run your program on the remote machine. It should stop executing at “Wait for
   attach”.
9. Start the debugger in VSCode. It should attach!

## Locally

Follow the same steps as above, but start from step 5.
