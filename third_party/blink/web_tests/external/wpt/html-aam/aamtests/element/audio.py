# Testing: https://w3c.github.io/aria/html-aam/#el-audio

TEST_HTML = "<audio id='target' controls></audio>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_AUDIO

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.AUDIO

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXGroup
#     # AXSubrole: AXAudio
#     # AXRoleDescription: "audio playback"
#     # Note: If the controls attribute is present, UI controls (e.g., play, volume) are exposed as descendants of an accessible object with a role of toolbar, and mapped as appropriate for the type of control (e.g., button or slider).

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: ROLE_SYSTEM_GROUPING

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Group
#     # Localized Control Type: "audio"
#     # Note: If the controls attribute is present, UI controls (e.g., play, volume) are exposed as children of the audio element in the accessibility tree, and mapped as appropriate for the type of control (e.g., button or slider).
#     # User agents MAY include the following in the accessibility tree and mark them as hidden or off-screen:
#     # - Loading messages or error messages
#     # - UI controls that are not currently displayed
