import json

from wptserve.utils import isomorphic_decode

def main(request, response):
    normalized = dict()

    for key, values in dict(request.headers).items():
        new_values = [isomorphic_decode(value) for value in values]
        normalized[isomorphic_decode(key.lower())] = new_values

    return (200, response.headers, json.dumps(normalized))
