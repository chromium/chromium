# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def init_graph(features):
    graph = {}
    for feature in features:
        graph[str(feature['name'])] = []
    return graph


def check_if_dependency_graph_contains_cycle(graph):
    state = {}
    visited, done = 0, 1

    def dfs(node):
        state[node] = visited
        for neighbor in graph.get(node, []):
            neighbor_state = state.get(neighbor)
            if neighbor_state == visited:
                raise Exception('Detected cycle in feature dependencies.')
            if neighbor_state == done:
                continue
            dfs(neighbor)
        state[node] = done

    for feature in graph:
        if feature not in state:
            dfs(feature)


# Marks a feature to be in origin trials if
# one of its dependencies is in origin trials.
def set_origin_trials_features(features, graph):
    in_origin_trial = set()

    def dfs(node):
        in_origin_trial.add(node)
        for neighbor in graph[node]:
            if neighbor not in in_origin_trial:
                dfs(neighbor)

    for feature in features:
        if feature['origin_trial_feature_name']:
            dfs(str(feature['name']))
    # Set 'in_origin_trial' for each feature.
    for feature in features:
        feature['in_origin_trial'] = True if str(feature['name']) in in_origin_trial else False
