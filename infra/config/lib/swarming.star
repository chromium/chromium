# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining Swarming resources."""

load("//lib/branches.star", "branches")

def root_permissions():
    """Sets up permissions that apply to all Chromium pools.

    Noop on a non-main branch, since Swarming pools are owned by the primary
    Chromium project defined on the main branch.
    """
    if not branches.matches(branches.selector.MAIN):
        return

    # Allow admins to cancel any task, delete bots, etc. in any Chromium pool.
    luci.binding(
        realm = "@root",
        roles = "role/swarming.poolOwner",
        groups = "project-chromium-admins",
    )

    # Allow everyone to see all tasks and bots in Chromium pools.
    luci.binding(
        realm = "@root",
        roles = "role/swarming.poolViewer",
        groups = "all",
    )

def pool_realm(
        *,
        name,
        extends = None,
        user_groups = None,
        user_users = None,
        user_projects = None,
        owner_groups = None):
    """Declares a realm with permissions for a Swarming pool.

    Individual Swarming pools are assigned to this realm in pools.cfg in
    Swarming server-side configs.

    Pools are owned by the main Chromium project and it makes sense to defined
    them only on the main branch. This declaration is noop on a non-main branch.

    Args:
        name: Name of the Swarming pool realm.
        extends: List of names of other realms whose permissions will be copied
            into this realm.
        user_groups: List of groups to give the "swarming.poolUser" role to.
        user_users: List of users to give the "swarming.poolUser" role to.
        user_projects: List of projects to give the "swarming.poolUser" role to.
        owner_groups: List of groups to give the "swarming.poolOwner" role to.
    """
    if not branches.matches(branches.selector.MAIN):
        return
    if not name.startswith("pools/"):
        fail("By convention Swarming pool realm name should start with pools/")

    luci.realm(
        name = name,
        extends = extends,
        bindings = [
            luci.binding(
                roles = "role/swarming.poolUser",
                groups = user_groups,
                users = user_users,
                projects = user_projects,
            ),
            luci.binding(
                roles = "role/swarming.poolOwner",
                groups = owner_groups,
            ),
        ],
    )

def task_accounts(*, realm, groups = None, users = None):
    """Declares what service accounts tasks in a realm can run as.

    Used to declare accounts for isolated tests. There's no need to separately
    declare accounts for Buildbucket builders since luci.builder(...) takes care
    of that itself.
    """
    luci.binding(
        realm = realm,
        roles = "role/swarming.taskServiceAccount",
        groups = groups,
        users = users,
    )

def task_triggerers(*, builder_realm, pool_realm, users = None, groups = None):
    """Declares who can launch arbitrary tasks.

    Used to allow end users to launch LUCI Editor (aka LED) tasks and isolated
    tests from their workstations.

    The given users will be allowed to submit tasks in the `builder_realm` realm
    (e.g. tasks that pretend to be "chromium/try" tasks), running on a Swarming
    pool in some `pool_realm` (e.g. "pools/try" or "pools/tests").

    Pools are owned by the main Chromium project, thus `pool_realm` setting is
    effective only on the main branch where pool realms are defined. It is
    silently skipped on on a non-main branch. Per-milestone projects still have
    builders, so `builder_realm` setting is always effective.
    """

    # Permission to submit tasks to Swarming at all.
    if branches.matches(branches.selector.MAIN):
        luci.binding(
            realm = pool_realm,
            roles = "role/swarming.poolUser",
            users = users,
            groups = groups,
        )

    # Permission to associated tasks with the builder realm.
    luci.binding(
        realm = builder_realm,
        roles = "role/swarming.taskTriggerer",
        users = users,
        groups = groups,
    )

swarming = struct(
    root_permissions = root_permissions,
    pool_realm = pool_realm,
    task_accounts = task_accounts,
    task_triggerers = task_triggerers,
)
