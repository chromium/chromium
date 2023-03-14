# Shared folder

This folder contains the code shared by several features. Please add code to
one of subfolder:
*    `coordinator/` if the code is *only* shared by coordinators/mediators
*    `model/` if the code is *only* shared by model objects
*    `ui/` if the code is *only* shared by ui elements
*    `public/` if the code is shared by all

Note that coordinators/mediators have access to all sub-folders, so if you are
adding code related to the UI but that coordinators/mediators will also use,
the correct folder is `ui/`.
