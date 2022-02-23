# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for enabling experiments on a per-recipe basis.

For each experiment registered for a recipe, if a builder that uses the recipe
does not have the experiment set, it will be set with the percentage specified
for the recipe.

See //recipes.star for information on setting experiments for a recipe.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "kinds")

RECIPE_EXPERIMENTS_KIND = "recipe_experiments"

def _recipe_experiments_key(recipe):
    return graph.key("@chromium", "", RECIPE_EXPERIMENTS_KIND, recipe)

def register_recipe_experiments(recipe, experiments):
    graph.add_node(_recipe_experiments_key(recipe), props = {
        "experiments": experiments,
    })

RECIPE_EXPERIMENTS_REF_KIND = "recipe_experiments_ref"

def _recipe_experiments_ref_key(bucket, builder):
    return graph.key("@chromium", "", kinds.BUCKET, bucket, RECIPE_EXPERIMENTS_REF_KIND, builder)

def register_recipe_experiments_ref(bucket, builder, recipe):
    key = _recipe_experiments_ref_key(bucket, builder)
    graph.add_node(key)
    graph.add_edge(key, _recipe_experiments_key(recipe))

def _set_recipe_experiments(ctx):
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to reformat properties")

    for bucket in cfg.buckets:
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            recipe_experiments_nodes = graph.children(_recipe_experiments_ref_key(bucket_name, builder_name), RECIPE_EXPERIMENTS_KIND)
            if len(recipe_experiments_nodes) != 1:
                fail("Each builder should refer to 1 recipe")
            recipe_experiments_node = recipe_experiments_nodes[0]
            for experiment, percentage in recipe_experiments_node.props.experiments.items():
                builder.experiments.setdefault(experiment, percentage)

lucicfg.generator(_set_recipe_experiments)
