# Overview
`wdotool.py` is a simple replacement for `xdotool` suitable for Wayland
desktops. It uses Gnome Remote Desktop and libEI APIs to synthesize keyboard
and mouse input.

# Setup instructions
The following instructions assume an externally-managed Python environment:

* Install Python dbus and venv:

  `sudo apt install python3-dbus python3-venv`

* Set up a virtual environment:

  This command creates a new directory called `venv` within your current
  directory:

  `python3 -m venv --system-site-packages venv`

* Activate it:

  `. venv/bin/activate`

* Install snegg (libEI Python bindings):

  `pip3 install git+http://gitlab.freedesktop.org/whot/snegg`

# Using the tool
* Activate the virtual environment:

  `. venv/bin/activate`

* Run:

  `./wdotool.py [options]`
  (specify the path if you're in a different directory from the script)

* Help:

  `./wdotool.py --help`

* Deactivate the virtual environment:

  When finished, deactivate the environment by closing the shell or by running
  this command:

  `deactivate`

# Examples
* Click at a location:

  `./wdotool.py --move_to 100 100 --click`

* Wait a few seconds to give the user time to select a window, then scroll it:

  `./wdotool.py --sleep 3 --scroll_discrete 1`

* Type "Hello World", assuming a US English keyboard for which 42 is the left
Shift key and the other constants below are the letters of "hello world":
  ```sh
  ./wdotool.py \
    --key_down 42 \
    --type 35 \
    --key_up 42 \
    --type 18 38 38 24 57 \
    --key_down 42 \
    --type 17 \
    --key_up 42 \
    --type 24 19 38 32
  ```
