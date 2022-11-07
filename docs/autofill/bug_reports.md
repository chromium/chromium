# Meaningful bug reports for Chrome Autofill

## Form filling

In case of problems with Chrome Autofill, please follow these steps to generate
meaningful bug reports:

1. Navigate to the form which causes problems.
2. Open chrome://autofill-internals in a separate tab.
3. Refresh the tab with the problematic form.
4. Reproduce the problem (e.g. click on the field that does not get filled,
   trigger autofilling the form, or enter data and submit the form if the
   problem is that Chrome does not offer saving the form data).
5. In the chrome://autofill-internals tab, press the "Download Log" button and
   attach it to a bug report. The log contains information about the website
   but not your personal data.
6. Please provide the URL (address) of the website that causes problems.

If you feel comfortable, please provide a screenshot, screen recording or save
the website. This simplifies our debugging. If these artifacts contain personal
information, please reach out to us (e.g. via a bug report) before attaching
them to a publicly visible bug report.

## Credit card filling/saving

In case you have any problems filling or saving credit cards, please *also*
share a screenshot of chrome://sync-internals/. Note that this contains your
email address so you may reach out to us first, before uploading this to
crbug.com/

## Password problems

Note that the password manager is architecturally a separate component. Use
chrome://password-manager-internals instead of chrome://autofill-internals.