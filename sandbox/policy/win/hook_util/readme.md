# IATHook

This directory contains a helper, IATHook, that allows placement
of hooks on particular loaded dlls by modifying their import
address table. This is very invasive and should be used with caution.

Consumers in chrome include //sandbox/policy and //chrome/chrome_elf.
As this is used very early in process startup it cannot use //base
or partition alloc. Tests are in //chrome/chrome_elf to validate
these constraints.
