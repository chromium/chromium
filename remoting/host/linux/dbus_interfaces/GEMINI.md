# D-Bus Interface Header Generation

This directory contains C++ headers generated from D-Bus interface definitions.
If you find that a required D-Bus interface header is missing, you can generate
it using the following steps.

## 1. Build the generator tool

The `gen_dbus_interface` tool is used to convert D-Bus XML introspection data
into C++ headers.

```bash
autoninja -C out/debug gen_dbus_interface
```

## 2. Introspect the D-Bus interface

Use `busctl` to obtain the XML definition of the interface. You will need to
decide whether to use the user bus (`--user`) or the system bus (`--system`).

*   **User bus:** Generally for desktop-session-related services (e.g.,
    `org.freedesktop.ScreenSaver`, GNOME services).
*   **System bus:** Generally for system-level services (e.g.,
    `org.freedesktop.login1`, NetworkManager).

If it is not clear which bus to use, **try the user bus first**, then the system
bus.

```bash
# Example for user bus
busctl --user introspect <bus_name> <object_path> --xml-interface > <interface_name>.xml

# Example for system bus
busctl --system introspect <bus_name> <object_path> --xml-interface > <interface_name>.xml
```

## 3. Generate the C++ header

Run the `gen_dbus_interface` tool with the generated XML.

```bash
out/debug/gen_dbus_interface --input=<interface_name>.xml --output=remoting/host/linux/dbus_interfaces/<header_name>.h
```

## 4. Clean up and Update Build Rules

1.  Add the new header file to the `sources` list in
    `remoting/host/linux/dbus_interfaces/BUILD.gn`.
2.  Add the appropriate copyright header to the generated file.
3.  Delete the temporary XML file.
4.  Run `git add <header_file>` so it's included in the index.
5.  Run `git cl format` to ensure the generated header matches the project's
    style.
