# SECURITY_OWNERS Policy

The Chromium project imposes additional requirements on the OWNERS of certain
security-sensitive areas of the codebase. Whether these requirements are met is
judged by a council of senior security engineers, who are listed in
[../../SECURITY_OWNERS](the root SECURITY_OWNERS file).

The specific requirements are:
1. The account being listed must be protected by mandatory 2-factor auth.
2. There must be a benefit to the project that outweighs the risk of giving
   another user access to approve particularly security-sensitive changes.

To add a new user to a SECURITY_OWNERS file anywhere in the tree, prepare a CL
adding that user to the file, then send it to one of the members of
`//SECURITY_OWNERS` for review as normal. The root `SECURITY_OWNERS` will
discuss amongst themselves, then either approve or disapprove your CL.
