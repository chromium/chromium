USE_PYTHON3 = True

def CheckChangeOnUpload(input_api, output_api):
    """Bypass all checks on upload."""
    return []

def CheckChangeOnCommit(input_api, output_api):
    """Bypass all checks on commit."""
    return []