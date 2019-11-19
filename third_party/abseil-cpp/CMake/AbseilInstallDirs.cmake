include(GNUInstallDirs)

# absl_VERSION is only set if we are an LTS release being installed, in which
# case it may be into a system directory and so we need to make subdirectories
# for each installed version of Abseil.  This mechanism is implemented in
# Abseil's internal Copybara (https://github.com/google/copybara) workflows and
# isn't visible in the CMake buildsystem itself.

if(absl_VERSION)
  set(ABSL_SUBDIR "${PROJECT_NAME}_${PROJECT_VERSION}")
  set(ABSL_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}/${ABSL_SUBDIR}")
  set(ABSL_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${ABSL_SUBDIR}")
  set(ABSL_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/{ABSL_SUBDIR}")
  set(ABSL_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${ABSL_SUBDIR}")
else()
  set(ABSL_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
  set(ABSL_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
  set(ABSL_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
  set(ABSL_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()