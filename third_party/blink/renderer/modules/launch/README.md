# Launch Events API

This directory contains the renderer side implementation of the client window [launch event API](https://github.com/WICG/sw-launch/blob/master/explainer.md).

> Note: This API is under active development, and is a work in progress.

## APIs in this Directory

This API consists of the following parts:
- `LaunchParams`: These are the parameters that the renderer was launched with. Currently, only file launches are supported but this will be extended in the future.
- `getLaunchParams`: An entry point for getting the cause of a launch from a client window.