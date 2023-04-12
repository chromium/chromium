# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for enabling experiments on a per-recipe basis.

For each experiment registered for a recipe, if a builder that uses the recipe
does not have the experiment set, it will be set with the percentage specified
for the recipe.

See //recipes.star for information on setting experiments for a recipe.
"""

load("@stdlib//internal/graph.star", "graph")
load("./nodes.star", "nodes")

_RECIPE_EXPERIMENTS = nodes.create_unscoped_node_type("recipe_experiments")

_RECIPE_EXPERIMENTS_REF = nodes.create_bucket_scoped_node_type("recipe_experiments_ref")

def register_recipe_experiments(recipe, experiments):
    _RECIPE_EXPERIMENTS.add(recipe, props = {
        "experiments": experiments,
    })

def register_recipe_experiments_ref(bucket, builder, recipe):
    key = _RECIPE_EXPERIMENTS_REF.add(bucket, builder)
    graph.add_edge(key, _RECIPE_EXPERIMENTS.key(recipe))

def _set_recipe_experiments(ctx):
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
            recipe_experiments_nodes = graph.children(_RECIPE_EXPERIMENTS_REF.key(bucket_name, builder_name), _RECIPE_EXPERIMENTS.kind)
            if len(recipe_experiments_nodes) != 1:
                fail("Each builder should refer to 1 recipe")
            recipe_experiments_node = recipe_experiments_nodes[0]
            for experiment, percentage in recipe_experiments_node.props.experiments.items():
                builder.experiments.setdefault(experiment, percentage)

lucicfg.generator(_set_recipe_experiments)
