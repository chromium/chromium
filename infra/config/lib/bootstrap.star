# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for handling details for bootstrapping builder properties.

Enabling bootstrapping for a builder provides versioning of the properties:
property changes for CQ builders will be applied during the CQ run and when
building a revision of code, the properties that are applied will be the
properties that were set for the builder at that revision.

This is accomplished by the use of a passthrough-style bootstrapper luciexe. The
properties specified in the starlark for the builder are written out to a
separate file. The builder's definition in cr-buildbucket.cfg is updated to use
the bootstrapper executable and the builder is updated to use the bootstrapper
and have properties set that the bootstrapper consumes. These properties enable
the bootstrapper to apply the properties from the properties file and then
execute the exe that was specified for the builder.

To enable bootstrapping for a builder, bootstrap must be set to True in its
builder definition its and it must be using a bootstrappable recipe. See
//recipes.star for more information on bootstrappable recipes.
"""

load("./nodes.star", "nodes")
load("//project.star", "settings")

# builder_config.star and orchestrator.star have generators that modify
# properties, so load them first to ensure that the modified properties get
# written out to the property files
load("./builder_config.star", _ = "builder_config")  # @unused
load("./orchestrator.star", _2 = "register_orchestrator")  # @unused

POLYMORPHIC = "POLYMORPHIC"

_NON_BOOTSTRAPPED_PROPERTIES = [
    # The led_recipes_tester recipe examines the recipe property in the input
    # properties of the build definition retrieved using led to determine which
    # builders' recipes are affected by the change. Bootstrapped properties
    # won't appear in the retrieved build definition, so don't bootstrap this.
    "recipe",

    # Sheriff-o-Matic queries for builder_group in the input properties to find
    # builds for the main sheriff rotation and Findit reads the builder_group
    # from the input properties of an analyzed build to set the builder group
    # for the target builder when triggering the rerun builder. Bootstrapped
    # properties don't appear in the build's input properties, so don't
    # bootstrap this property.
    # TODO(gbeaty) When finalized input properties are exported to BQ, remove
    # this.
    "builder_group",

    # Sheriff-o-Matic will query for sheriff_rotations in the input properties
    # to determine which sheriff rotation a build belongs to. Bootstrapped
    # properties don't appear in the build's input properties, so don't
    # bootstrap this property.
    # TODO(gbeaty) When finalized input properties are exported to BQ, remove
    # this.
    "sheriff_rotations",
]

# Nodes for storing the ability of recipes to be bootstrapped
_RECIPE_BOOTSTRAPPABILITY = nodes.create_unscoped_node_type("recipe_bootstrappability")

# Nodes for storing bootstrapping information about builders
_BOOTSTRAP = nodes.create_bucket_scoped_node_type("bootstrap")

def register_recipe_bootstrappability(name, bootstrappability):
    if bootstrappability not in (False, True, POLYMORPHIC):
        fail("bootstrap must be one of False, True or POLYMORPHIC")
    if bootstrappability:
        _RECIPE_BOOTSTRAPPABILITY.add(name, props = {
            "bootstrappability": bootstrappability,
        })

def register_bootstrap(bucket, name, bootstrap, executable):
    """Registers the bootstrap for a builder.

    Args:
        bucket: The name of the bucket the builder belongs to.
        name: The name of the builder.
        bootstrap: Whether or not the builder should actually be bootstrapped.
        executable: The builder's executable.
    """

    # The properties file will be generated for any bootstrappable recipe so
    # that if a builder is switched to be bootstrapped its property file exist
    # even if an earlier revision is built (to a certain point). The bootstrap
    # property of the node will determine whether the builder's properties are
    # overwritten to actually use the bootstrapper.
    _BOOTSTRAP.add(bucket, name, props = {
        "bootstrap": bootstrap,
        "executable": executable,
    })

def _bootstrap_properties(ctx):
    """Update builder properties for bootstrapping.

    For builders whose recipe supports bootstrapping, their properties will be
    written out to a separate file. This is done even if the builder is not
    being bootstrapped so that the properties file will exist already when it is
    flipped to being bootstrapped.

    For builders that have opted in to bootstrapping, this file will be read at
    build-time and update the build's properties with the contents of the file.
    The builder's properties within the buildbucket configuration will be
    modified with the properties that control the bootstrapper itself.

    The builders that have opted in to bootstrapping is determined by examining
    the lucicfg graph to find a bootstrap node for a given builder. These nodes
    will be added by the builder function. This is done rather than writing out
    the properties file in the builder function so that the bootstrapped
    properties have any final modifications that luci.builder would perform
    (merging module-level defaults, setting global defaults, etc.).
    """
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to reformat properties")

    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            bootstrap_node = _BOOTSTRAP.get(bucket_name, builder_name)
            if not bootstrap_node or bootstrap_node.props.bootstrap == False:
                continue
            executable = bootstrap_node.props.executable
            recipe_bootstrappability_node = _RECIPE_BOOTSTRAPPABILITY.get(executable)
            if not recipe_bootstrappability_node:
                continue

            builder_properties = json.decode(builder.properties)
            bootstrapper_args = []

            if recipe_bootstrappability_node.props.bootstrappability == POLYMORPHIC:
                non_bootstrapped_properties = builder_properties

                # TODO(gbeaty) Once all builder specs are migrated src-side,
                # remove -properties-optional
                bootstrapper_args = ["-polymorphic", "-properties-optional"]

            else:
                non_bootstrapped_properties = {}

                for p in _NON_BOOTSTRAPPED_PROPERTIES:
                    if p in builder_properties:
                        non_bootstrapped_properties[p] = builder_properties[p]

                properties_file = "builders/{}/{}/properties.json".format(bucket_name, builder_name)
                non_bootstrapped_properties["$bootstrap/properties"] = {
                    "top_level_project": {
                        "repo": {
                            "host": "chromium.googlesource.com",
                            "project": "chromium/src",
                        },
                        "ref": settings.ref,
                    },
                    "properties_file": "infra/config/generated/{}".format(properties_file),
                }
                ctx.output[properties_file] = json.indent(json.encode(builder_properties), indent = "  ")

            if bootstrap_node.props.bootstrap:
                non_bootstrapped_properties.update({
                    "$bootstrap/exe": {
                        "exe": json.decode(proto.to_jsonpb(builder.exe, use_proto_names = True)),
                    },
                    "led_builder_is_bootstrapped": True,
                })

                builder.properties = json.encode(non_bootstrapped_properties)

                builder.exe.cipd_package = "infra/chromium/bootstrapper/${platform}"
                builder.exe.cipd_version = "latest"
                builder.exe.cmd = ["bootstrapper"] + bootstrapper_args

lucicfg.generator(_bootstrap_properties)
